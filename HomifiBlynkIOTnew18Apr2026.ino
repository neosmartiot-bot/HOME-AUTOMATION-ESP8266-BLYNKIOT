  /****************************************************************
   * Neo-Smart Industrial Firmware v0.1.0
   * Target: ESP8266 (ESP-01 Compatible)
   * Features:
   *  - 2 Relay Control
   *  - TRIAC Fan Speed Control (Zero Cross + Timer)
   *  - Blynk IoT Integration
   *  - EEPROM Persistence
   *  - Non-blocking Architecture
   *  - DEBUG Serial Logs (Disable for ESP-01)
   *  - Blynk RTC Time Sync
   ****************************************************************/
  
  /************************************************************
   * Project       : <Home Automation Solutions>
   * File Name     : <HomifiBlynkIOT02Apr2026.ino>
   * Authorrrrr    : <Saad Ilyas>
   * Organization  : <Neo-Smart>
   *
   * Description   :
   *  <Brief technical description of what this file/module does.
   *   Mention key functionality, interfaces, or hardware used.>
   *
   * Hardware used :
   *   MCU used    : <ESP8266, ESP-01 CHIP with a custom made
   *                  circuit developed by Neo-Smart>
   *   Peripherals : <Double Relay Module with a TRIAC for fan>
   *
   * Version       : v0.1.0
   * Dateeee       : <02-Apr-2026>
   *
   * Revision History:
   *   ----------------------------------------------------------
   *   Version        Date           Author        Description
   *   ----------------------------------------------------------
   *   v0.1.0    <02-Apr-2026>    <Saad ilyas>    Initial release
   *   v0.2.0    <date>              <name>       <Changes made>
   *   v0.3.0    <date>              <name>       <Changes made>
   *   v0.4.0    <date>              <name>       <Changes made>
   *
   * Dependencies :
   *   - <EEPROM.h>
   *   - <TimeLib.h>
   *   - <WidgetRTC.h>
   *   - <BlynkEdgent.h>
   *
   * Important Notes :
   *   - <Important implementation notes>
   *   This firmware is specifically meant for 28BYJ MOTOR
   *   - <Limitations or assumptions>
   *   User must count limitations ULN2003 driver IC, which
   *   contains darlington pairs transistors
   *
   * *********************************************************/
  
  #define BLYNK_TEMPLATE_ID            "TMPLcmNcMa75"
  #define BLYNK_TEMPLATE_NAME      "IOT HOME AUTOMATION"
  #define BLYNK_FIRMWARE_VERSION          "0.1.0"
  
  // #define BLYNK_PRINT Serial
  
  #include "BlynkEdgent.h"
  #include <WidgetRTC.h>
  #include "hw_timer.h"
  #include <TimeLib.h>
  #include <EEPROM.h>
  
  // #define APP_DEBUG
  // #define BLYNK_DEBUG
  // #define USE_NODE_MCU_BOARD
  
/////////// -------------------- Debug Control -------------------- ////////////
  
  #define DEBUG 0
  #if DEBUG
    #define DBG(x) Serial.print(x)
    #define DBGLN(x) Serial.println(x)
  #else
    #define DBG(x)
    #define DBGLN(x)
  #endif

///////////// -------------------- Debug Blynk -------------------- ////////////

  #define BLYNK_DEBUG 1

  #if BLYNK_DEBUG
    #define BLOG(x)    {terminal.print(x);}
    #define BLOGLN(x)  {terminal.println(x); terminal.flush();}
  #else
    #define BLOG(x)
    #define BLOGLN(x)
  #endif
  
////////// -------------------- Pins & addresses -------------------- //////////
  
  #define RELAY1_PIN    0   // GPIO0(D3)
  #define RELAY2_PIN    2   // GPIO2(D4)
  #define ZEROCR_PIN    1   // GPIO1(TX)
  #define TRIAC1_PIN    3   // GPIO3(RX)
  
  #define EEPROM_ADDR_FAN  901
  #define EEPROM_ADDR_RL1  902
  #define EEPROM_ADDR_RL2  903
  
////////// ------------------------ Blynk RTC ----------------------- //////////
  
  WidgetRTC rtc;
  WidgetTerminal terminal(V10);
  unsigned long lastRequest = 0;
  
////////// ------------------ Volatile ISR Variables -----------------//////////

  volatile uint32_t zcCount = 0;

  volatile bool zcDetected = false;
  
  // volatile uint32_t lastZC = 0;
  volatile uint32_t lastZCtime = 0;
  volatile uint32_t rejectCount = 0;
  volatile uint32_t halfCycleTime = 10000;   // default 10ms 
  
////////// -------------------- Runtime Variables --------------------//////////
  
  String currentTime;
  String currentDate;

  int Slider_Value = 0;

  int brightness = 1;
  int directionn = 1;   // +1 = increasing, -1 = decreasing
  
  uint8_t Relay1State = LOW;
  uint8_t Relay2State = LOW;
  
  uint8_t targetBrightness = 255;
  uint8_t currentBrightness = 100;
  
  unsigned long lastRampTime = 0;
  
////////////////////////////////////////////////////////////////////////////////
///////// -------------------- Function Prototypes --------------------/////////
////////////////////////////////////////////////////////////////////////////////
  
  void loadEEPROM();
  void saveEEPROM();
  
  void runACCycle();
  
  void pushTimeBlynk();
  
  void updateBrightness();

  void ICACHE_RAM_ATTR dimTimerISR();
  void ICACHE_RAM_ATTR zeroCrossISR();
  
////////////////////////////////////////////////////////////////////////////////
/////////// -------------------- Blynk Functions --------------------///////////
////////////////////////////////////////////////////////////////////////////////
  
  BLYNK_CONNECTED()
   {// Blynk.syncAll();}
    
    rtc.begin();
    BLOGLN("[BLYNK] Connected");

    BLOGLN("[WIFI] Mode=" + String(WiFi.getMode()));
    BLOGLN("[WIFI] SSID=" + WiFi.SSID());
  
    Blynk.sendInternal("rtc", "sync");
    
    // Blynk.syncVirtual(V0);   // Date
    // Blynk.syncVirtual(V1);   // Time
    // Blynk.syncVirtual(V2);   // Temper
    // Blynk.syncVirtual(V3);   // Humidi 
    
    // Blynk.syncVirtual(V4);   // Relay1
    // Blynk.syncVirtual(V5);   // Relay2
    Blynk.syncVirtual(V6);}  // Fan Rl
  
BLYNK_WRITE(V4)
 {Relay1State = param.asInt();
  digitalWrite(RELAY1_PIN, Relay1State);
  BLOGLN("[BLYNK] Relay1: " + String(Relay1State));
  saveEEPROM();}
  
BLYNK_WRITE(V5)
 {Relay2State = param.asInt();
  digitalWrite(RELAY2_PIN, Relay2State);
  BLOGLN("[BLYNK] Relay2: " + String(Relay2State));
  saveEEPROM();}
  
BLYNK_WRITE(V6)
 /*{targetBrightness = param.asInt();
 BLOGLN("[BLYNK] Fan Slider: " + String(targetBrightness));
 saveEEPROM();}*/
 {currentBrightness = param.asInt();
  // currentBrightness = targetBrightness;
  BLOGLN("[BLYNK] Fan Slider: " + String(currentBrightness));
  saveEEPROM();
  
  if (currentBrightness == 0)
     {// digitalWrite(TRIAC1_PIN, 0);
      BLOGLN("[BLYNK] Low Brightness: " + String(currentBrightness));}
  else
     {// digitalWrite(TRIAC1_PIN, 1);
      BLOGLN("[BLYNK] High Brightness: " + String(currentBrightness));}}
     
 /*{Slider_Value = param.asInt(); 
  if (Slider_Value>0)
     {targetBrightness = Slider_Value;
      BLOGLN("[BLYNK] Fan Slider: " + String(targetBrightness));}}*/
  
void pushTimeBlynk()
 {currentTime = String(hour()) + ":" + minute() + ":" + second();
  currentDate = String(day()) + " " + month() + " " + year();
    
  Blynk.virtualWrite(V0, currentDate);
  Blynk.virtualWrite(V1, currentTime);}
  
////////////////////////////////////////////////////////////////////////////////
/////////// -------------------- Setup Function --------------------////////////
////////////////////////////////////////////////////////////////////////////////
  
void setup()
 {EEPROM.begin(1024);
  // Serial.begin(115200);
  
  BLOGLN("\n[BOOT] System Starting...");
  
  pinMode(ZEROCR_PIN, INPUT);
    
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
    
  pinMode(TRIAC1_PIN, OUTPUT);

  // digitalWrite(RELAY1_PIN, HIGH);
  // digitalWrite(RELAY2_PIN, HIGH);
  
  digitalWrite(TRIAC1_PIN, 0);
    
  loadEEPROM();
  
  // // // // // //BLOGLN("[INIT] EEPROM Loaded");
  // // //BLOGLN("Relay1=" + String(Relay1State));
  // // //BLOGLN("Relay2=" + String(Relay2State)); 
  // BLOGLN("Dimmer=" + String(targetBrightness));
  
  digitalWrite(RELAY1_PIN, Relay1State);
  digitalWrite(RELAY2_PIN, Relay2State);

  delay(2000);
  
  // attachInterrupt(digitalPinToInterrupt(ZEROCR_PIN), zeroCrossISR, RISING);

  attachInterrupt(ZEROCR_PIN, zeroCrossISR, RISING);
  
  hw_timer_init(NMI_SOURCE, 0);
  hw_timer_set_func(dimTimerISR);

  // WiFi.persistent(true);
  // WiFi.setAutoReconnect(true);
  // WiFi.mode(WIFI_STA);
  
  BlynkEdgent.begin();
  
  BLOGLN("[WIFI] Mode=" + String(WiFi.getMode()));
  BLOGLN("[WIFI] SSID=" + WiFi.SSID());
  BLOGLN("[WIFI] PSSK=" + WiFi.psk());
    
  BLOGLN("Relay1=" + String(Relay1State));
  BLOGLN("Relay2=" + String(Relay2State));
    
  BLOGLN("Dimmer=" + String(targetBrightness));}
  
////////////////////////////////////////////////////////////////////////////////
//////////// -------------------- Loop Function --------------------////////////
////////////////////////////////////////////////////////////////////////////////
  
void loop()
 {BlynkEdgent.run();

 ////// ------ Run multiple AC cycles at current brightness ------ //////

 // {runACCycle();
 //  updateBrightness();}   // update brightness slowly (independent)

 static uint32_t lastZC = 0;
 static uint32_t lastPrint = 0;
  
 //////// ---------- Print RTC time every 1 second ---------- //////////
 
 if (millis() - lastRequest > 1000)
    {lastRequest = millis();
     pushTimeBlynk();}}

 ///////// ---------- Print zcCount every 2 seconds ---------- /////////
  
 // -- if (millis() % 2000 < 50) // modulo use a bit sloppy method -- //
 
 /*if (millis() - lastPrint > 1000)
      {lastPrint = millis();
   
       // BLOGLN("ZC Count: " + String(zcCount));
       // BLOGLN("ZC: " + String(zcCount) + " Reject: " + String(rejectCount));
     
       BLOGLN("ZC Count / Sec: " + String(zcCount - lastZC));
       lastZC = zcCount;} */

 // BLOGLN("[ZC] freq=" + String(freq) + " halfCycle=" + String(halfCycleTime));

 ///////// ------------ Run this function in loop ------------ /////////

 // runACCycle();} 



////////////////////////////////////////////////////////////////////////////////
////////// ----------------- Run AC Cycle Function ------------------ //////////
////////////////////////////////////////////////////////////////////////////////



/*void runACCycle() 
  {if (zcDetected)
  
     {zcDetected = false;

      if (currentBrightness == 0)
         {digitalWrite(TRIAC1_PIN, 0);}    // full OFF

      else if (currentBrightness >= 255)   // full ONN
              {digitalWrite(TRIAC1_PIN, 1);}

      else
         {GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
          os_delay_us(100);
          GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}

  /////////////// ---------- safety clamp ---------- ///////////////
      
      // if (delayMicros < 300) delayMicros = 300;
      // if (delayMicros > (halfCycleTime - 300))
      //     delayMicros = halfCycleTime - 300;

      // hw_timer_disarm();
      // hw_timer_arm(delayMicros);}}*/

 /*  
  {unsigned long t0 = micros();

  // int localBrightness = brightness;  // snapshot (CRITICAL FIX)

  if (brightness > 0)
  
      // ALWAYS USE PHASE CONTROL (NO BLIND OFF ZONE)
     {// int dimDelay = map(brightness, 5, 250, 8000, 200);

      int dimDelay = 200 + ((255 - brightness) * 30);

      // ---- safety clamp (VERY IMPORTANT) ---- //
  
      if (dimDelay > 9000) dimDelay = 9000;
      if (dimDelay < 200) dimDelay = 200;

      delayMicroseconds(dimDelay);
        
      GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
      delayMicroseconds(100);
      GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}*/



  /*if (brightness <= 5)          // FULL OFF → do nothing
     {}
  
  else if (brightness >= 250)     // FULL ON → fire immediately
          {dimDelay = 200;
           delayMicroseconds(dimDelay);

           GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
           delayMicroseconds(80);   // gate pulse width
           GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}
  else
     
     {dimDelay = map(brightness, 5, 250, 8000, 800);
      delayMicroseconds(dimDelay);

      GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
      delayMicroseconds(80);   // gate pulse width
      GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}*/

  //// ---------- WAIT UNTIL END OF HALF CYCLE ---------- ////

  /*while (micros() - t0 < 10000);}   // lock to 10ms half-cycle */

////////////////////////////////////////////////////////////////////////////////
//////////// ------------------ Update Brightness ------------------////////////
////////////////////////////////////////////////////////////////////////////////

/*void updateBrightness()

 ////////// -------------- SLOW RAMP CONTROL -------------- //////////
 
 {static uint32_t lastUpdate = 0;  

  if (millis() - lastUpdate < 5000) return;  // speed control // stable 50Hz relationship // 20
     {lastUpdate = millis();

      // Example: hold fixed brightness OR change slowly
      // brightness = 200;   // <-- HOLD VALUE HERE

       // OR controlled ramp:
       // brightness++;
       // brightness--;
       }
       // brightness += directionn;}  

  if (brightness <= 0)
     {brightness = 0;
      directionn = +1;}
           
  else if (brightness >= 255)                 // Change direction at limits
          {brightness = 255;
           directionn = -1;}}*/
  
////////////////////////////////////////////////////////////////////////////////
//////////// -------------------- ISR Functions --------------------////////////
////////////////////////////////////////////////////////////////////////////////
  
void ICACHE_RAM_ATTR zeroCrossISR()

 {if (zcDetected)
 
 {uint32_t now = micros();

  uint32_t dt = now - lastZCtime;

  static uint32_t stableDelay = 5000;

  static uint8_t stableCounter = 0;
  
  //// ---- validate AC half cycle (50Hz safe window) ---- ////
  
  /*if (dt < 4000 || dt > 16000) // ignore noise
       {rejectCount++;
        return;}*/
      
  /*if (dt > 8000 || dt < 12000)
     
       {stableCounter++;

        if (stableCounter > 50)   // update every ~0.5 sec
           {halfCycleTime = dt;
            stableCounter = 0;}}*/
  
  /////// ---------- simple low-pass filter ---------- ///////

  // halfCycleTime = (halfCycleTime * 7 + dt) / 8;
  // halfCycleTime = (halfCycleTime * 15 + dt) / 16;

  /// ------ Adaptive delay based on real AC timing ------ ///

  uint32_t delayMicros = (halfCycleTime * (255 - currentBrightness)) / 255;

  // dimDelay = getTriacDelay(currentBrightness);
  // hw_timer_arm(dimDelay);
  
  // uint32_t delayMicros = 30 * (255 - currentBrightness) + 400;

  //// ------ only update if change is significant ------ ////

  // if (abs((int)delayMicros - (int)stableDelay) > 50)
     // {stableDelay = delayMicros;}

  ////////// ------------- smooth it -------------- //////////

  // stableDelay = (stableDelay * 7 + delayMicros) / 8;
  // stableDelay = (stableDelay * 15 + delayMicros) / 16;

  ////////// ------------ CLAMP TIGHT ------------- //////////
  
  if (delayMicros < 300) delayMicros = 300;
  if (delayMicros > (halfCycleTime - 300))
      delayMicros = halfCycleTime - 300;

  ////////// ---------- ARM THE TIMER NOW ---------- /////////
  
  hw_timer_arm(delayMicros);
  // hw_timer_arm(stableDelay);
      
  // zcCount++;
  
  lastZCtime = now;
  
  halfCycleTime = dt;
 
  zcDetected = false;}}
 
/*{if (!zerocFlag)
      {zerocFlag = 1;
       
       if (currentBrightness > 1 && currentBrightness < 255)
          {// digitalWrite(TRIAC1_PIN, 0);
          
           int dimDelay = 30 * (255 - currentBrightness) + 400;

           hw_timer_arm(dimDelay);}  



      if (currentBrightness <= 10)
         {zerocFlag = 0;                  // FULL OFF → do nothing
          return;}
 
       else if (currentBrightness >= 255) // FULL ON → immediate firing
 
               {GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
                os_delay_us(100);
                GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}
       else
          {GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
           os_delay_us(100);
           GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));
      
           zerocFlag = 0;
           return;}}                      // FULL OFF → do nothing*/

void ICACHE_RAM_ATTR dimTimerISR()
 /*{if (currentBrightness > targetBrightness) // || currentBrightness > 0)   // || (state == 0 && curBrightness > 0))
       {--currentBrightness;}
    else if (currentBrightness < targetBrightness && currentBrightness < 255 && Slider_Value > 1)
            {++currentBrightness;}*/
            
{if (currentBrightness <= 0)         // FULL OFF → do nothing
    {digitalWrite(TRIAC1_PIN, 0);}
     // return;

 else if (currentBrightness >= 255)   // FULL ONN → immediate firing

         {digitalWrite(TRIAC1_PIN, 1);}
 else
         {GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
          os_delay_us(100);
          GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}
      
  zcDetected = true;}
      
  ////////// ---------- second reinforcement pulse ---------- //////////
  
  /*os_delay_us(200);
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
  os_delay_us(100);
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}*/

////////////////////////////////////////////////////////////////////////////////
////////// ---------------- Gamma Correction Function --------------- //////////
////////////////////////////////////////////////////////////////////////////////

/*uint16_t getTriacDelay(uint8_t brightness)
 {if (brightness <= 10)     // OFF zone
      return 10000;        // effectively no firing
      
  if (brightness >= 250)   // FULL ON zone
      return 0;

  // Non-linear curve (gamma ≈ 2.2)
  float normalized = brightness / 255.0;
  float gammaCorrected = normalized * normalized;  // simple gamma

  uint16_t delay = (1.0 - gammaCorrected) * halfCycleTime;

  // safety clamps
  if (delay < 300) delay = 300;
  if (delay > (halfCycleTime - 500)) delay = halfCycleTime - 500;

  return delay;}*/

////////////////////////////////////////////////////////////////////////////////
///////////// -------------------- Load EEPROM --------------------/////////////
////////////////////////////////////////////////////////////////////////////////
  
void loadEEPROM()
  
 {Relay1State = EEPROM.read(EEPROM_ADDR_RL1);
  Relay2State = EEPROM.read(EEPROM_ADDR_RL2);

  EEPROM.get(EEPROM_ADDR_FAN, currentBrightness);}
  
////////////////////////////////////////////////////////////////////////////////
///////////// -------------------- Save EEPROM --------------------/////////////
////////////////////////////////////////////////////////////////////////////////
  
void saveEEPROM()
  
 {EEPROM.write(EEPROM_ADDR_RL1, Relay1State);
  EEPROM.write(EEPROM_ADDR_RL2, Relay2State);
    
  EEPROM.put(EEPROM_ADDR_FAN, currentBrightness);
    
  EEPROM.commit();
    
  BLOGLN("[EEPROM] Saved State");}
