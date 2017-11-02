/*
  ESP12E - Webserver and fading LED
    - Fading LED on ESP12E IO-pin
    - By an implemented webserver paramters of the LED can be changed

  Author: Udo Esser (24.09.2017)
*/

#include "Skulls_Eyes.h"

// ___________________________________ setup ________________________________________
// - Standard Arduino/ESP8266 setup function
// __________________________________________________________________________________
void setup() {
  sint32_t     t_timeDiff  = 0;
  time_t       t_localTime = 0;
  tmElements_t tm;
  
  Serial.begin(115200);
  delay(10);

  g_rstInfo = system_get_rst_info();

  if ((g_logSer = logger.regLogDestSerial(DEBUG, LOG_SERIAL)) < 0) {
    Serial.println();
    Serial.println("Register Serial logger failed!");
  }
  if ((g_logWifi = logger.regLogDestWifi(DEBUG, C_LOG_HOST, C_LOG_PORT, "/log", String(C_HOSTNAME) + ".log", 
                                         "logLev", "logFct", "logStr", "logStrln")) < 0) {
    Serial.println();
    Serial.println("Register Wifi logger failed!");
  }

  wifiConnect();
  
  loadConfig();

  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
    
    ntpConnect();
    delay(1000);
    t_localTime = getNtpTime();
  }

  t_localTime = getLocalTime();

  if (g_rstInfo->reason != REASON_DEEP_SLEEP_AWAKE || g_skullsEyes.wakeupTime == 0) {
    breakTime(t_localTime, tm);

    tm.Hour   = (g_skullsEyes.wakeupHour == 0) ? C_WAKEUP_HOUR[tm.Month] : g_skullsEyes.wakeupHour;
    tm.Minute = 0;
    tm.Second = 0;
    
    g_skullsEyes.wakeupTime = makeTime(tm); 
    g_skullsEyes.sleepTime  = (g_skullsEyes.stayAliveHours == 0) ? 0 : 
                               g_skullsEyes.wakeupTime + g_skullsEyes.stayAliveHours * SECS_PER_HOUR;

    if (t_localTime > g_skullsEyes.wakeupTime) {
      g_skullsEyes.wakeupTime += SECS_PER_DAY;
    }
    if (g_skullsEyes.sleepTime > 0 && t_localTime > g_skullsEyes.sleepTime) {
      g_skullsEyes.sleepTime += SECS_PER_DAY;
    }
  }
 
  g_Max_PWM = (g_skullsEyes.dimmer == 100) ? C_MAX_PWM : g_skullsEyes.dimmer * 2.7;
  g_LED_brightness_factor = pow((double) g_Max_PWM, (double) 1 / g_skullsEyes.steps);

  logger.logln(DEBUG, "SETUP", "");
  logger.logln(DEBUG, "SETUP", "Reason for wakeup or (re)start:");
  logger.logln(DEBUG, "SETUP", "g_rstInfo->reason = " + String(g_rstInfo->reason));
  logger.logln(DEBUG, "SETUP", String("  - means: ") + C_REASON_CODE[g_rstInfo->reason]);
  logger.logln(DEBUG, "SETUP", "");
  logger.logln(DEBUG, "SETUP", String("g_Max_PWM               = ") + g_Max_PWM);
  logger.logln(DEBUG, "SETUP", String("g_skullsEyes.steps      = ") + g_skullsEyes.steps);
  logger.logln(DEBUG, "SETUP", String("g_LED_brightness_factor = ") + g_LED_brightness_factor);
  logger.logln(DEBUG, "SETUP", String("g_skullsEyes.ledDelayMS = ") + g_skullsEyes.ledDelayMS);
  logger.logln(DEBUG, "SETUP", "");

  sprintf(g_logStr, "local time approx: %02d.%02d.%04d - %02d:%02d:%02d",
                    day(g_skullsEyes.localTimeApprox), month(g_skullsEyes.localTimeApprox), 
                    year(g_skullsEyes.localTimeApprox), hour(g_skullsEyes.localTimeApprox), 
                    minute(g_skullsEyes.localTimeApprox), second(g_skullsEyes.localTimeApprox));
  logger.logln(DEBUG, "SETUP", g_logStr);
  sprintf(g_logStr, "local time       : %02d.%02d.%04d - %02d:%02d:%02d",
                    day(t_localTime), month(t_localTime), year(t_localTime),
                    hour(t_localTime), minute(t_localTime), second(t_localTime));
  logger.logln(DEBUG, "SETUP", g_logStr);
  sprintf(g_logStr, "wakup time       : %02d.%02d.%04d - %02d:%02d:%02d",
                    day(g_skullsEyes.wakeupTime), month(g_skullsEyes.wakeupTime), 
                    year(g_skullsEyes.wakeupTime), hour(g_skullsEyes.wakeupTime), 
                    minute(g_skullsEyes.wakeupTime), second(g_skullsEyes.wakeupTime));
  logger.logln(DEBUG, "SETUP", g_logStr);
  sprintf(g_logStr, "sleep time       : %02d.%02d.%04d - %02d:%02d:%02d",
                    day(g_skullsEyes.sleepTime), month(g_skullsEyes.sleepTime), 
                    year(g_skullsEyes.sleepTime), hour(g_skullsEyes.sleepTime), 
                    minute(g_skullsEyes.sleepTime), second(g_skullsEyes.sleepTime));
  logger.logln(DEBUG, "SETUP", g_logStr);
  logger.logln(DEBUG, "SETUP", String("g_skullsEyes.localTimeApprox: ") + g_skullsEyes.localTimeApprox);
  logger.logln(DEBUG, "SETUP", String("t_localTime                 : ") + t_localTime);
  logger.logln(DEBUG, "SETUP", String("g_skullsEyes.wakeupTime     : ") + g_skullsEyes.wakeupTime);
  logger.logln(DEBUG, "SETUP", String("g_skullsEyes.sleepTime      : ") + g_skullsEyes.sleepTime);

  if (g_rstInfo->reason == REASON_DEEP_SLEEP_AWAKE) {
    t_timeDiff = g_skullsEyes.wakeupTime - t_localTime;

    logger.logln(DEBUG, "SETUP", String("t_timeDiff (WakeupTime - LocalTime): ") + t_timeDiff);

    if (t_timeDiff > 0) {
      uint32_t ul_sleepSecs = (t_timeDiff > C_MAX_SLEEP_SECS) ? C_MAX_SLEEP_SECS : t_timeDiff;
      
      t_localTime = getLocalTime();  // to get new localTimeApprox
      g_skullsEyes.localTimeApprox += ul_sleepSecs;  // localTimeApprox will be at next wakeup
      saveConfig();

      if (ul_sleepSecs == C_MAX_SLEEP_SECS)
        ul_sleepSecs += C_RTC_CORRECTION;  // esp8266 wakes up ~150 secs to early if letting it sleep for 1 hour
      logger.logln(DEBUG, "SETUP", String("ESP.deepSleep(") + ul_sleepSecs + " sec.)");
      ESP.deepSleep(ul_sleepSecs * 1000 * 1000);
      delay(5000);
    }
    else {
      g_skullsEyes.wakeupTime += SECS_PER_DAY;
      saveConfig();
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    setupWebserver();
  } 

  pinMode(LED_GPIO_PIN, OUTPUT); // Port als Ausgang schalten

  g_LED_ticker.attach_ms(g_skullsEyes.ledDelayMS, LEDFadingTimerCB);
}

// ___________________________________ loop _________________________________________
// - Standard Arduino/ESP8266 loop function 
// - handles client calls and reconnects to WiFi if connection is lost
// __________________________________________________________________________________
void loop() {
  time_t t_localTime = 0;

  static unsigned long sul_millis = 0;
  
  ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED) {
    logger.logln(DEBUG, "LOOP", "");
    logger.logln(DEBUG, "LOOP", "WiFi connection lost, trying to reconnect");

    if (wifiConnect()) {
      setupOTA();
      setupWebserver();
    }
    else
      delay(5000); 
  }

  if (! g_OTAInProgress) {
    webServer.handleClient();

    if (sul_millis == 0 || millis() - sul_millis > C_CHECK_SLEEP_MS) {
      sul_millis = millis();      

      t_localTime = getLocalTime();
    
      if (g_skullsEyes.wakeupTime == 0) {
        tmElements_t tm;
        
        breakTime(t_localTime, tm);
  
        tm.Hour   = (g_skullsEyes.wakeupHour == 0) ? C_WAKEUP_HOUR[tm.Month] : g_skullsEyes.wakeupHour;
        tm.Minute = 0;
        tm.Second = 0;
        
        g_skullsEyes.wakeupTime = makeTime(tm); 
        g_skullsEyes.sleepTime  = (g_skullsEyes.stayAliveHours == 0) ? 0 : 
                                   g_skullsEyes.wakeupTime + g_skullsEyes.stayAliveHours * SECS_PER_HOUR;
    
      }

      if (t_localTime > g_skullsEyes.wakeupTime) {
        g_skullsEyes.wakeupTime += SECS_PER_DAY;
      }

      logger.logln(DEBUG, "LOOP", "");
      sprintf(g_logStr, "local time approx: %02d.%02d.%04d - %02d:%02d:%02d",
                        day(g_skullsEyes.localTimeApprox), month(g_skullsEyes.localTimeApprox), 
                        year(g_skullsEyes.localTimeApprox), hour(g_skullsEyes.localTimeApprox), 
                        minute(g_skullsEyes.localTimeApprox), second(g_skullsEyes.localTimeApprox));
      logger.logln(DEBUG, "LOOP", g_logStr);
      sprintf(g_logStr, "local time       : %02d.%02d.%04d - %02d:%02d:%02d",
                        day(t_localTime), month(t_localTime), year(t_localTime),
                        hour(t_localTime), minute(t_localTime), second(t_localTime));
      logger.logln(DEBUG, "LOOP", g_logStr);
      sprintf(g_logStr, "wakup time       : %02d.%02d.%04d - %02d:%02d:%02d",
                        day(g_skullsEyes.wakeupTime), month(g_skullsEyes.wakeupTime), 
                        year(g_skullsEyes.wakeupTime), hour(g_skullsEyes.wakeupTime), 
                        minute(g_skullsEyes.wakeupTime), second(g_skullsEyes.wakeupTime));
      logger.logln(DEBUG, "LOOP", g_logStr);
      sprintf(g_logStr, "sleep time       : %02d.%02d.%04d - %02d:%02d:%02d",
                        day(g_skullsEyes.sleepTime), month(g_skullsEyes.sleepTime), 
                        year(g_skullsEyes.sleepTime), hour(g_skullsEyes.sleepTime), 
                        minute(g_skullsEyes.sleepTime), second(g_skullsEyes.sleepTime));
      logger.logln(DEBUG, "LOOP", g_logStr);
      logger.logln(DEBUG, "LOOP", String("g_skullsEyes.localTimeApprox: ") + g_skullsEyes.localTimeApprox);
      logger.logln(DEBUG, "LOOP", String("t_localTime                 : ") + t_localTime);
      logger.logln(DEBUG, "LOOP", String("g_skullsEyes.wakeupTime     : ") + g_skullsEyes.wakeupTime);
      logger.logln(DEBUG, "LOOP", String("g_skullsEyes.sleepTime      : ") + g_skullsEyes.sleepTime);

      if (g_skullsEyes.sleepTime > 0 && t_localTime > g_skullsEyes.sleepTime) {
        g_skullsEyes.sleepTime += SECS_PER_DAY;

        sprintf(g_logStr, "new sleep time: %02d.%02d.%04d - %02d:%02d:%02d",
                          day(g_skullsEyes.sleepTime), month(g_skullsEyes.sleepTime), 
                          year(g_skullsEyes.sleepTime), hour(g_skullsEyes.sleepTime), 
                          minute(g_skullsEyes.sleepTime), second(g_skullsEyes.sleepTime));
        logger.logln(DEBUG, "LOOP", g_logStr);
        logger.logln(DEBUG, "LOOP", String("new g_skullsEyes.sleepTime: ") + g_skullsEyes.sleepTime);
  
        t_localTime = getLocalTime();  // to get new localTimeApprox
        g_skullsEyes.localTimeApprox += C_MAX_SLEEP_SECS;  // localTimeApprox will be at next wakeup
        saveConfig();
  
        logger.logln(DEBUG, "LOOP", String("ESP.deepSleep(") + String((C_MAX_SLEEP_SECS + C_RTC_CORRECTION)) + 
                                    " sec.)");

        ESP.deepSleep((C_MAX_SLEEP_SECS + C_RTC_CORRECTION) * 1000 * 1000);
        delay(5000);
      }
    }
  
//    delay(1000);
  }
}

// ___________________________________ wifiConnect __________________________________
// - connects to WiFi
// __________________________________________________________________________________
boolean wifiConnect() {
  logger.logln(g_logSer, DEBUG, "SETUP", "");
  logger.logln(g_logSer, DEBUG, "SETUP", String("----------------------------------------------------"));
  logger.logln(g_logSer, DEBUG, "SETUP", String("ESP8266 (re)starting or reconnecting after WiFi lost"));
  logger.logln(g_logSer, DEBUG, "SETUP", "");
  logger.logln(g_logSer, DEBUG, "SETUP", String("Connecting to AP '") + C_SSID + "': ");

  WiFi.hostname(C_HOSTNAME);
  WiFi.mode(WIFI_STA); // Als Station an einen vorhanden Access Ppoint anmelden
//  wifi_set_sleep_type(LIGHT_SLEEP_T);
  WiFi.begin(C_SSID, C_PWD);

  unsigned short conn_tics = 0;
  while (WiFi.status() != WL_CONNECTED && conn_tics < 20) {
    conn_tics++;
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    IPAddress IPAddr = WiFi.localIP();
    
    logger.logln(g_logWifi, DEBUG, "SETUP", "");
    logger.logln(g_logWifi, DEBUG, "SETUP", String("----------------------------------------------------"));
    logger.logln(g_logWifi, DEBUG, "SETUP", String("ESP8266 (re)starting or reconnecting after WiFi lost"));
    logger.logln(g_logWifi, DEBUG, "SETUP", "");
    logger.logln(g_logWifi, DEBUG, "SETUP", String("Connecting to AP: ") + C_SSID);

    logger.logln(DEBUG, "SETUP", String("Time to connect: ") + conn_tics * 500 + " ms");
    logger.logln(DEBUG, "SETUP", String("IP-Adr: ") + IPAddr[0] + "." + IPAddr[1] + "." + IPAddr[2] + "." + IPAddr[3]);
//    WiFi.printDiag(Serial);

    if (! MDNS.begin(C_HOSTNAME)) {  // Start the mDNS responder for skulleyes.local
      logger.logln(DEBUG, "SETUP", "Error setting up MDNS responder!");
    } 
    else {
      logger.logln(DEBUG, "SETUP", String("mDNS responder started - Hostname: ") + C_HOSTNAME + ".local");
    }
  }
  else {
    logger.logln(DEBUG, "SETUP", "");
    logger.logln(DEBUG, "SETUP", String("Connecting to AP '") + C_SSID + "' failed!");
  }

  return (WiFi.status() == WL_CONNECTED);
}

// ___________________________________ setupOTA _____________________________________
// - setup of OTA stuff
// __________________________________________________________________________________
void setupOTA() {
  logger.logln(DEBUG, "SETUP", "");
  logger.logln(DEBUG, "SETUP", String("setup OTA - hostname: ") + C_HOSTNAME);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(C_HOSTNAME);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    g_OTAInProgress = true;
    Serial.println("");
    Serial.println("OTA - Start");
  });
  ArduinoOTA.onEnd([]() {
    g_OTAInProgress = false;
    Serial.println("");
    Serial.println("OTA - End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    g_OTAInProgress = false;
    Serial.printf("Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  logger.logln(DEBUG, "SETUP", "OTA begin");
}

// ___________________________________ ntpConnect ___________________________________
// - setup of ntp server
// __________________________________________________________________________________
void ntpConnect() {
  logger.logln(DEBUG, "SETUP", ""); 
  logger.logln(DEBUG, "SETUP", String("Connecting to ntp server: ") + C_NTP_SERVER_NAME);

  UDP.begin(123);  // Start listening for UDP messages on port 123

  logger.logln(DEBUG, "SETUP", String("Local port: ") + UDP.localPort());

  setSyncProvider(getNtpTime);
  setSyncInterval(C_NTP_SYNC_INTERVAL);
}

// ___________________________________ sendNTPpacket ________________________________
// - sends ntp request via UDP
// __________________________________________________________________________________
void sendNTPpacket(IPAddress& address) {
  memset(g_NTPBuffer, 0, C_NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  
  g_NTPBuffer[0] = 0b11100011;  // Initialize values needed to form NTP request: LI, Version, Mode
  g_NTPBuffer[1] = 0;     // Stratum, or type of clock
  g_NTPBuffer[2] = 6;     // Polling Interval
  g_NTPBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  g_NTPBuffer[12] = 49;
  g_NTPBuffer[13] = 0x4E;
  g_NTPBuffer[14] = 49;
  g_NTPBuffer[15] = 52;

  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(g_NTPBuffer, C_NTP_PACKET_SIZE);
  UDP.endPacket();
}

// ___________________________________ getNtpTime ___________________________________
// - gets the actual time calculated by the ntp time
// __________________________________________________________________________________
time_t getNtpTime() {
  const uint32_t C_SEVENTY_YEARS = 2208988800UL;

  uint32_t ui_ntpTime  = 0;
  time_t   t_timeUNIX  = 0;

  while (UDP.parsePacket() > 0) ; // discard any previously received packets

//    logger.logln(DEBUG, "TIME", "");
//    logger.logln(DEBUG, "TIME", "Sending NTP request ...");
  
  if (! WiFi.hostByName(C_NTP_SERVER_NAME, g_timeServerIP)) {  // Get the IP address of the NTP server
//    logger.logln(DEBUG, "TIME", "DNS lookup failed!");
  }
  else {
//    logger.logln(DEBUG, "TIME", String("Time server IP: ") + g_timeServerIP[0] + "." + g_timeServerIP[1] + "." + 
//                                                g_timeServerIP[2] + "." + g_timeServerIP[3]);

    sendNTPpacket(g_timeServerIP);
  
    uint32_t ui_beginWait = millis();
    while (millis() - ui_beginWait < 1000) {
      if (UDP.parsePacket() >= C_NTP_PACKET_SIZE) {
//        logger.logln(DEBUG, "TIME", "Receive NTP Response");
        
        UDP.read(g_NTPBuffer, C_NTP_PACKET_SIZE);  // read packet into the buffer
  
        // convert four bytes starting at location 40 to a long integer
        ui_ntpTime = (g_NTPBuffer[40] << 24) | (g_NTPBuffer[41] << 16) | 
                     (g_NTPBuffer[42] <<  8) |  g_NTPBuffer[43];
  
        t_timeUNIX = ui_ntpTime - C_SEVENTY_YEARS;
      }
    }
  
    if (t_timeUNIX == 0) {
//      logger.logln(DEBUG, "TIME", "No NTP Response!");
   }
    else {
/*
      sprintf(g_logStr, "UTC time: %02d:%02d:%02d", 
                        hour(t_timeUNIX), minute(t_timeUNIX), second(t_timeUNIX));
      logger.logln(DEBUG, "TIME", g_logStr);
      logger.logln(DEBUG, "TIME", "");
*/
    }
  }
  
  return t_timeUNIX;
}

// ___________________________________ getLocalTime _________________________________
// - gets the local time
// __________________________________________________________________________________
time_t getLocalTime() {
  static TimeChangeRule   CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // Central European Summer Time
  static TimeChangeRule   CET  = { "CET",  Last, Sun, Oct, 3,  60 };  // Central European Standard Time
  static Timezone         CE(CEST, CET);
  static TimeChangeRule * tcr;             //pointer to the time change rule, use to get the TZ abbrev
  static unsigned long    sul_millis = 0;
  
  g_skullsEyes.localTimeApprox += (millis() - sul_millis) / 1000;
  sul_millis = millis();      

  time_t t_timeLOCAL = CE.toLocal(now(), &tcr);
  
  logger.logln(DEBUG, "TIME", "");
  logger.logln(DEBUG, "TIME", "Before check TimeLocal - TimeLocalApprox deviation:");
  sprintf(g_logStr, "local time approx: %02d.%02d.%04d - %02d:%02d:%02d",
                    day(g_skullsEyes.localTimeApprox), month(g_skullsEyes.localTimeApprox), 
                    year(g_skullsEyes.localTimeApprox), hour(g_skullsEyes.localTimeApprox), 
                    minute(g_skullsEyes.localTimeApprox), second(g_skullsEyes.localTimeApprox));
  logger.logln(DEBUG, "TIME", g_logStr);
  sprintf(g_logStr, "local time       : %02d.%02d.%04d - %02d:%02d:%02d",
                    day(t_timeLOCAL), month(t_timeLOCAL), year(t_timeLOCAL),
                    hour(t_timeLOCAL), minute(t_timeLOCAL), second(t_timeLOCAL));
  logger.logln(DEBUG, "TIME", g_logStr);

  if ((g_skullsEyes.localTimeApprox - 3600) > t_timeLOCAL || (g_skullsEyes.localTimeApprox + 3600) < t_timeLOCAL) {
    t_timeLOCAL = g_skullsEyes.localTimeApprox;
  
    logger.logln(DEBUG, "TIME", "Deviation TimeLocal - TimeLocalApprox more than 1 hour, so use LocalTimeApprox");
  }
  
  g_skullsEyes.localTimeApprox = t_timeLOCAL;

  logger.logln(DEBUG, "TIME", "After check TimeLocal - TimeLocalApprox deviation:");
  sprintf(g_logStr, "local time approx: %02d.%02d.%04d - %02d:%02d:%02d",
                    day(g_skullsEyes.localTimeApprox), month(g_skullsEyes.localTimeApprox), 
                    year(g_skullsEyes.localTimeApprox), hour(g_skullsEyes.localTimeApprox), 
                    minute(g_skullsEyes.localTimeApprox), second(g_skullsEyes.localTimeApprox));
  logger.logln(DEBUG, "TIME", g_logStr);
  sprintf(g_logStr, "local time       : %02d.%02d.%04d - %02d:%02d:%02d",
                    day(t_timeLOCAL), month(t_timeLOCAL), year(t_timeLOCAL),
                    hour(t_timeLOCAL), minute(t_timeLOCAL), second(t_timeLOCAL));
  logger.logln(DEBUG, "TIME", g_logStr);

  saveConfig();

  return t_timeLOCAL;
}

// ___________________________________ setupWebserver _______________________________
// - setup of Webserver
// __________________________________________________________________________________
void setupWebserver() {
  logger.logln(DEBUG, "SETUP", "");
  logger.logln(DEBUG, "SETUP", "setup Webserver...");

  webServer.on("/", wsHandler_skull_eyes_parameters);
  webServer.on("/skullseyes", wsHandler_skull_eyes_parameters);
  webServer.onNotFound(wsNotFoundHandler);
  webServer.begin();

  logger.logln(DEBUG, "SETUP", "Webserver started, listening on Port 80");
}

// ___________________________________ LEDFadingTimerCB _____________________________
// - Controls the fading effect of the LED
// __________________________________________________________________________________
void LEDFadingTimerCB() {
  static int     si_LED_Step = 0;
  static int     si_extra_delay = 0;
  static boolean sb_LED_fade_up = true;

  int i_LED_PWM_value = 0;
 
  if (g_timer_reset) {
    g_timer_reset = false;
    si_LED_Step = 0;
    si_extra_delay = 0;
    sb_LED_fade_up = true;
  }
  
  if (si_extra_delay > 0) {
//    logger.logln(DEBUG, "TIMER", String("si_extra_delay = ") + si_extra_delay);
    si_extra_delay--;
    return;
  }
  
  i_LED_PWM_value = (int) pow(g_LED_brightness_factor, (double) si_LED_Step);
  
/*
  logger.logln(DEBUG, "TIMER", "");
  logger.logln(DEBUG, "TIMER", String("si_LED_Step     = ") + si_LED_Step);
  logger.logln(DEBUG, "TIMER", String("i_LED_PWM_value = ") + i_LED_PWM_value);
  logger.logln(DEBUG, "TIMER", String("sb_LED_fade_up  = ") + (sb_LED_fade_up) ? "True" : "False");
*/
  
  analogWrite(LED_GPIO_PIN, (int) i_LED_PWM_value);

  sb_LED_fade_up ? si_LED_Step++ : si_LED_Step--;

  if (si_LED_Step > g_skullsEyes.steps) {
    sb_LED_fade_up = false;
    si_LED_Step = g_skullsEyes.steps;
    si_extra_delay = g_skullsEyes.ledExtraDelayTop;
    return;
  }
  if (si_LED_Step < 0) {
    sb_LED_fade_up = true;
    si_LED_Step = 0;
    si_extra_delay = g_skullsEyes.ledExtraDelayBottom;
    return;
  }
}

// ___________________________________ buildHTML ____________________________________
// - Builds up the HTML website
// __________________________________________________________________________________
char * buildHTML(uint16_t steps, uint16_t ledDelayMS, uint16_t ledExtraDelayTop, 
                 uint16_t ledExtraDelayBottom, uint16_t dimmer, uint16_t control, 
                 uint16_t wakeupHour, uint16_t stayAliveHours) {
  static char * s_html;
  
  if (! s_html) {
    s_html = (char *) malloc(strlen(g_skullsEyesHTML) + 100);
  }

  if (s_html) {
    snprintf(s_html, strlen(g_skullsEyesHTML) + 100, g_skullsEyesHTML, C_HOSTNAME,
             steps, ledDelayMS, ledExtraDelayTop, ledExtraDelayBottom, dimmer,
             (control == 0) ? "checked" : "", (control == 1) ? "checked" : "",
             (control == 2) ? "checked" : "", wakeupHour, stayAliveHours);
  }
  else {
    s_html = (char *) C_NE_SPACE_HTML;
  }

  return s_html;
}

// ___________________________________ wsHandler_skull_eyes_parameters ______________
// - Creates website and implements rest-service
// __________________________________________________________________________________
void wsHandler_skull_eyes_parameters() {
  for (int i = 0; i < webServer.args(); i++) {
    if (webServer.argName(i) == "Control")
      if (webServer.arg(i) == "On")
        g_skullsEyes.control = 0;
      else if (webServer.arg(i) == "Off")
        g_skullsEyes.control = 1;
      else if (webServer.arg(i) == "Fade")
        g_skullsEyes.control = 2;
      else
        g_skullsEyes.control = 2;
    if (webServer.argName(i) == "Dimmer")
      g_skullsEyes.dimmer = webServer.arg(i).toInt();
    if (webServer.argName(i) == "Steps")
      g_skullsEyes.steps = webServer.arg(i).toInt();
    if (webServer.argName(i) == "Delay")
      g_skullsEyes.ledDelayMS = webServer.arg(i).toInt();
    if (webServer.argName(i) == "Delay_Top")
      g_skullsEyes.ledExtraDelayTop = webServer.arg(i).toInt();
    if (webServer.argName(i) == "Delay_Bottom")
      g_skullsEyes.ledExtraDelayBottom = webServer.arg(i).toInt();
    if (webServer.argName(i) == "Wakeup")
      g_skullsEyes.wakeupHour = webServer.arg(i).toInt();
    if (webServer.argName(i) == "Stay_Alive")
      g_skullsEyes.stayAliveHours = webServer.arg(i).toInt();
    if (webServer.argName(i) == "Button" && webServer.arg(i) == "Sleep") {
      time_t t_localTime = getLocalTime();  // to get new localTimeApprox
      g_skullsEyes.localTimeApprox += C_MIN_SECS;  // localTimeApprox will be at next wakeup
      saveConfig();
      logger.logln(DEBUG, "HTTP", String("ESP.deepSleep(") + C_MIN_SECS + " sec.)");
      ESP.deepSleep(C_MIN_MICROSECS);
      delay(5000);
    }
  }

  if (webServer.args() > 0) {
    saveConfig();

    g_Max_PWM               = (g_skullsEyes.dimmer == 100) ? C_MAX_PWM : g_skullsEyes.dimmer * 2.7;
    g_LED_brightness_factor = pow((double) g_Max_PWM, (double) 1 / g_skullsEyes.steps);

    g_LED_ticker.detach();
    g_timer_reset = true;

    if (g_skullsEyes.control == 0) {
      analogWrite(LED_GPIO_PIN, g_Max_PWM);
    }
    else if (g_skullsEyes.control == 2) {
      if (g_skullsEyes.dimmer > 0)
        g_LED_ticker.attach_ms(g_skullsEyes.ledDelayMS, LEDFadingTimerCB);
      else
        analogWrite(LED_GPIO_PIN, 0);
    }
    else {
      analogWrite(LED_GPIO_PIN, 0);
    }

    tmElements_t tm;
    time_t t_localTime = getLocalTime();
    
    breakTime(t_localTime, tm);
    tm.Hour   = (g_skullsEyes.wakeupHour == 0) ? C_WAKEUP_HOUR[tm.Month] : g_skullsEyes.wakeupHour;
    tm.Minute = 0;
    tm.Second = 0;
    
    g_skullsEyes.wakeupTime = makeTime(tm); 
    g_skullsEyes.sleepTime  = (g_skullsEyes.stayAliveHours == 0) ? 0 : 
                               g_skullsEyes.wakeupTime + g_skullsEyes.stayAliveHours * SECS_PER_HOUR;

    if (t_localTime > g_skullsEyes.wakeupTime) {
      g_skullsEyes.wakeupTime += SECS_PER_DAY;
    }
    if (g_skullsEyes.sleepTime > 0 && t_localTime > g_skullsEyes.sleepTime) {
      g_skullsEyes.sleepTime += SECS_PER_DAY;
    }

    logger.logln(DEBUG, "HTTP", "");
    logger.logln(DEBUG, "HTTP", "WebServer Handler 'wsHandler_skull_eyes_parameters':");
    
    logDetails();
    
    logger.logln(DEBUG, "HTTP", String("g_LED_brightness_factor = ") + g_LED_brightness_factor);
    sprintf(g_logStr, "wakup time: %02d.%02d.%04d - %02d:%02d:%02d",
                      day(g_skullsEyes.wakeupTime), month(g_skullsEyes.wakeupTime), 
                      year(g_skullsEyes.wakeupTime), hour(g_skullsEyes.wakeupTime), 
                      minute(g_skullsEyes.wakeupTime), second(g_skullsEyes.wakeupTime));
    logger.logln(DEBUG, "HTTP", g_logStr);
    sprintf(g_logStr, "sleep time: %02d.%02d.%04d - %02d:%02d:%02d",
                      day(g_skullsEyes.sleepTime), month(g_skullsEyes.sleepTime), 
                      year(g_skullsEyes.sleepTime), hour(g_skullsEyes.sleepTime), 
                      minute(g_skullsEyes.sleepTime), second(g_skullsEyes.sleepTime));
    logger.logln(DEBUG, "HTTP", g_logStr);
    logger.logln(DEBUG, "HTTP", String("g_skullsEyes.wakeupTime: ") + g_skullsEyes.wakeupTime);
    logger.logln(DEBUG, "HTTP", String("g_skullsEyes.sleepTime:  ") + g_skullsEyes.sleepTime);
  }

  webServer.send(200, "text/html", buildHTML(g_skullsEyes.steps, g_skullsEyes.ledDelayMS, 
                                             g_skullsEyes.ledExtraDelayTop, 
                                             g_skullsEyes.ledExtraDelayBottom, g_skullsEyes.dimmer,
                                             g_skullsEyes.control, g_skullsEyes.wakeupHour, 
                                             g_skullsEyes.stayAliveHours));
}

// ___________________________________ wsNotFoundHandler ____________________________
// - Is called on any not defined URL
// __________________________________________________________________________________
void wsNotFoundHandler() {
  logger.logln(DEBUG, "HTTP", "");
  logger.logln(DEBUG, "HTTP", "Not Found Handler");

  logDetails();

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";

  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " " + webServer.argName ( i ) + ": " + webServer.arg ( i ) + "\n";
  }

  webServer.send(404, "text/plain", message);
}

// ___________________________________ logDetails ___________________________________
// - Standard Arduino/ESP8266 setup function
// __________________________________________________________________________________
void logDetails() {
  String method = "Unknown";
  
  switch(webServer.method()) {
    case HTTP_GET:
      method = "GET";
      break;
    case HTTP_POST:
      method = "POST";
      break;
    case HTTP_PUT:
      method = "PUT";
      break;
    case HTTP_PATCH:
      method = "PATCH";
      break;
    case HTTP_DELETE:
      method = "DELETE";
      break;
  }

  logger.logln(DEBUG, "HTTP", String("URL is: ") + webServer.uri());
  logger.logln(DEBUG, "HTTP", String("HTTP Method on request was: ") + method);
  
  // Print how many properties we received and their names and values.
  logger.logln(DEBUG, "HTTP", String("Number of query properties: ") + webServer.args());
  for (int i = 0; i < webServer.args(); i++) {
    logger.logln(DEBUG, "HTTP", String("  - ") + webServer.argName(i) + " = " + webServer.arg(i));
  }
}

// ___________________________________ loadConfig ___________________________________
// - Loads config from RTC memory
// __________________________________________________________________________________
void loadConfig() {
  EEPROM.begin(512);
  EEPROM.get(0, g_skullsEyes);
  EEPROM.end();
  
  if (g_skullsEyes.saved != 0x1010) {
    logger.logln(DEBUG, "LOAD-CONFIG", "");
    logger.logln(DEBUG, "LOAD-CONFIG", "Load config failed or not saved last time !!!");

    g_skullsEyes.saved               = 0x1010;
    g_skullsEyes.control             =   2;  // 0: On, 1: Off, 2: Fade
    g_skullsEyes.ledDelayMS          =  80;
    g_skullsEyes.ledExtraDelayTop    =   4;
    g_skullsEyes.ledExtraDelayBottom =   4;
    g_skullsEyes.steps               =  50;
    g_skullsEyes.dimmer              = 100;
    g_skullsEyes.wakeupHour          =  20;
    g_skullsEyes.stayAliveHours      =   1;
    g_skullsEyes.wakeupTime          =   0;
    g_skullsEyes.sleepTime           =   0;
    g_skullsEyes.localTimeApprox     =   0;
  }
}

// ___________________________________ saveConfig ___________________________________
// - Saves config to RTC memory
// __________________________________________________________________________________
void saveConfig() {
  EEPROM.begin(512);
  EEPROM.put(0, g_skullsEyes);
  delay(200);
  EEPROM.commit();
  EEPROM.end();

  logger.logln(DEBUG, "SAVE-CONFIG", "");
  logger.logln(DEBUG, "SAVE-CONFIG", "Config saved");
}

