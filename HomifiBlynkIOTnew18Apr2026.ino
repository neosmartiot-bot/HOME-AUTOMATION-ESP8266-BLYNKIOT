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

  volatile bool zcDetected = true;
  volatile bool systemReady = false;
  
  // volatile uint32_t lastZC = 0;
  volatile uint32_t lastZCtime = 0;
  volatile uint32_t rejectCount = 0;
  
  volatile uint32_t halfCycleTime = 10000;   // default 10ms 
  
////////// -------------------- Runtime Variables --------------------//////////

  const uint16_t fanDelay[10] = 
  {9000,   // OFF
   8000,
   7000,
   6000,
   5000,
   4000,
   3000,
   2000,
   1200,
   600};   // FULL
  
  String currentTime;
  String currentDate;

  int Slider_Value = 0;

  int brightness = 1;
  int directionn = 1;   // +1 = increasing, -1 = decreasing


   uint32_t dt;
   
  uint8_t Relay1State = LOW;
  uint8_t Relay2State = LOW;
  
  static bool timerArmed = false;
  
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
    
    Blynk.syncVirtual(V4);   // Relay1
    Blynk.syncVirtual(V5);   // Relay2
    Blynk.syncVirtual(V6);   // Fan Rl

    systemReady = true;}     // ENABLE AFTER SYNC
  
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
  BLOGLN("[BLYNK] Fan Slider: " + String(currentBrightness));
  saveEEPROM();}
     
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
  
  // pinMode(ZEROCR_PIN, INPUT_PULLUP);
  
  pinMode(ZEROCR_PIN, INPUT);
    
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
    
  pinMode(TRIAC1_PIN, OUTPUT);
  
  // digitalWrite(TRIAC1_PIN, 0);

  systemReady = false;
    
  loadEEPROM();

  // currentBrightness = 0;   // FORCE OFF at boot
  
  // // // // // //BLOGLN("[INIT] EEPROM Loaded");
  // // //BLOGLN("Relay1=" + String(Relay1State));
  // // //BLOGLN("Relay2=" + String(Relay2State)); 
  // BLOGLN("Dimmer=" + String(targetBrightness));

  // digitalWrite(RELAY1_PIN, HIGH);
  // digitalWrite(RELAY2_PIN, HIGH);
  
  digitalWrite(RELAY1_PIN, Relay1State);
  digitalWrite(RELAY2_PIN, Relay2State);
  currentBrightness = currentBrightness;

  delay(1000);
  
  // attachInterrupt(digitalPinToInterrupt(ZEROCR_PIN), zeroCrossISR, RISING);

  BlynkEdgent.begin();

  delay(1000);
  
  // if (Blynk.connected())
  
  attachInterrupt(ZEROCR_PIN, zeroCrossISR, RISING);
  // attachInterrupt(ZEROCR_PIN, zeroCrossISR, FALLING);
  
  hw_timer_init(NMI_SOURCE, 0);
  hw_timer_set_func(dimTimerISR);

  // WiFi.setAutoReconnect(true);
  // WiFi.persistent(true);
  // WiFi.mode(WIFI_STA);
  
  // BlynkEdgent.begin();
  
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

 ///////// ---------- Print zcCount every 1 second ---------- /////////
  
 // -- if (millis() % 2000 < 50) // modulo use a bit sloppy method -- //
 
 if (millis() - lastPrint > 1000)
    {lastPrint = millis();
     pushTimeBlynk();         // included pushTime function in this also
     
     // BLOGLN("ZC Count: " + String(zcCount));
     // BLOGLN("ZC: " + String(zcCount) + " Reject: " + String(rejectCount));
     
     BLOGLN("ZC Count / Sec: " + String(zcCount - lastZC));
     BLOGLN("HalfCycleTime=" + String(halfCycleTime));
     lastZC = zcCount;}

 static uint32_t lastReconnectAttempt = 0;
     
 if (!Blynk.connected())
    {if (millis() - lastReconnectAttempt > 10000)
        {lastReconnectAttempt = millis();

        // Step 1: ensure WiFi is alive
        if (WiFi.status() != WL_CONNECTED)
           {WiFi.disconnect();
            WiFi.begin();}

        // Step 2: reconnect Blynk
        Blynk.connect(1000);}}
        }   // timeout 1 sec
  
////////////////////////////////////////////////////////////////////////////////
//////////// -------------------- ISR Functions --------------------////////////
////////////////////////////////////////////////////////////////////////////////
  
void ICACHE_RAM_ATTR zeroCrossISR()

 {zcCount++;
 
      // uint16_t zcOffset = 100; // microseconds (tune this)
      timerArmed = true;
      uint32_t now = micros();
      // uint32_t dt = now - lastZCtime;
      // dt = now - lastZCtime;
      halfCycleTime = now - lastZCtime;
      lastZCtime = now;

      timerArmed = false;   // ALWAYS release for next cycle
      
      static uint32_t stableDelay = 5000;
      static uint8_t stableCounter = 0;
  
   //// ---- validate AC half cycle (50Hz safe window) ---- ////
  
   if (halfCycleTime < 8000)  // reject only very fast noise
      {// rejectCount++;
       return;}

   if (halfCycleTime > 20000) // reject only very fast noise
      {hw_timer_arm(1000);} // force minimal firing to keep TRIAC alive
       
   /*if (dt > 8000 || dt < 12000)
     
        {stableCounter++;

         if (stableCounter > 50)   // update every ~0.5 sec
            {halfCycleTime = dt;
             stableCounter = 0;}}*/
  
   /////// ---------- simple low-pass filter ---------- ///////

   // halfCycleTime = (halfCycleTime * 7 + dt) / 8;
   // halfCycleTime = (halfCycleTime * 15 + dt) / 16;

   /// ------ Adaptive delay based on real AC timing ------ ///

   // uint32_t delayMicros = 30 * (255 - currentBrightness) + 400; // old formula
  
   // uint32_t delayMicros = (halfCycleTime * (255 - currentBrightness)) / 255;

   int index = map(currentBrightness, 0, 255, 0, 9);
   uint16_t delayMicros = fanDelay[index];

   // --- Non-linear curve (gamma ≈ 2.2) --- //
  
   /*float normalized = currentBrightness / 255.0;
   float gammaCorrected = normalized * normalized;   // simple gamma

   uint16_t delayMicros = (1.0 - gammaCorrected) * halfCycleTime;*/

   //// ------ only update if change is significant ------ ////

   // if (abs((int)delayMicros - (int)stableDelay) > 50)
      // {stableDelay = delayMicros;}

   ////////// ------------- smooth it -------------- //////////

   // stableDelay = (stableDelay * 7 + delayMicros) / 8;
   // stableDelay = (stableDelay * 15 + delayMicros) / 16;

   //////////// ---------- safety clamp ---------- ////////////
  
   if (delayMicros < 300) delayMicros = 300;
   if (delayMicros > (halfCycleTime - 600))   // (delayMicros > (halfCycleTime - 300))
       delayMicros = halfCycleTime - 600;     // delayMicros = halfCycleTime - 300;

   ////////// ---------- ARM THE TIMER NOW ---------- /////////
   
   // if (!timerArmed)
      // {timerArmed = true;
       // hw_timer_arm(delayMicros + zcOffset);
       hw_timer_arm(delayMicros);
       // hw_timer_arm(stableDelay);
   
   }

void ICACHE_RAM_ATTR dimTimerISR()
 
 {if (!systemReady)
     {currentBrightness = 100;}
        
      if (currentBrightness <= 10) // || (!systemReady)) // FULL OFF → do nothing
         {digitalWrite(TRIAC1_PIN, 0);}
          // return;}

      else if (currentBrightness >= 245)       // FULL ONN → immediate firing

              {digitalWrite(TRIAC1_PIN, 1);}
               // return;}
               /*GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
               os_delay_us(150); // 100 for bulb 150 for fan
               GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));
               return;}*/
      else
              {GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
               os_delay_us(100); // 100 for bulb 150 for fan
               GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}}

  ////////// ---------- second reinforcement pulse ---------- //////////
  
  /*os_delay_us(200);
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
  os_delay_us(100);
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}*/

////////////////////////////////////////////////////////////////////////////////
//////////// ------------- Brightness Control Function -------------////////////
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
////////// ---------------- Gamma Correction Function --------------- //////////
////////////////////////////////////////////////////////////////////////////////

/*uint16_t getTriacDelay(uint8_t brightness)
 {if (brightness <= 10)    // OFF zone
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
