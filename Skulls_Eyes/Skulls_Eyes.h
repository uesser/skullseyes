extern "C" {
#include "user_interface.h"
}

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <EEPROM.h>
#include <ESP8266Logger.h>

#define LED_GPIO_PIN 12 // GPIO-12

//
// Constant variables
//
const char *        C_SSID              = "Bazinga";
const char *        C_PWD               = "amIwelcome?";

const char *        C_HOSTNAME          = "esp8266test";

const char *        C_LOG_HOST          = "192.168.0.19";
const char *        C_LOG_PORT          = "3000";

const char *        C_NTP_SERVER_NAME   = "de.pool.ntp.org";
const int           C_NTP_PACKET_SIZE   =   48;  // NTP time stamp is in the first 48 bytes of the message
const unsigned int  C_NTP_SYNC_INTERVAL =  300;  // 300 sec. = 5 min.

const unsigned int  C_MAX_PWM           = 1000;

const unsigned long C_MAX_SLEEP_SECS    = 3600;
const unsigned long C_RTC_CORRECTION    =  150;
const unsigned long C_CHECK_SLEEP_MS    =   60 * 1000;
const unsigned long C_MIN_SECS          =   60;
const unsigned long C_MIN_MICROSECS     =   60 * 1000 * 1000;

const unsigned int  C_WAKEUP_HOUR[]     = {0, 18, 18, 19, 21, 22, 22, 22, 22, 21, 20, 18, 17};

const char *        C_NE_SPACE_HTML     = "Can't allocate memory for webpage";

const String        C_REASON_CODE[7]    = {"normal startup by power on",
                                           "hardware watch dog reset",
                                           "exception reset, GPIO status not changed",
                                           "software watch dog reset, GPIO status not changed",
                                           "software restart, system_restart, GPIO status not changed",
                                           "wake up from deep-sleep",
                                           "external system reset", 
                                          };

//
// Global parameter variables - Can be changed by rest-service
//
typedef struct {
  uint16_t saved;
  uint16_t control;             // 0: On, 1: Off, 2: Fade
  uint16_t ledDelayMS;
  uint16_t ledExtraDelayTop;
  uint16_t ledExtraDelayBottom;
  uint16_t steps;
  uint16_t dimmer;
  uint16_t wakeupHour;
  uint16_t stayAliveHours;
  time_t   wakeupTime;
  time_t   sleepTime;
  time_t   localTimeApprox;
} skullsEyes_t;

// Global variables
skullsEyes_t     g_skullsEyes;
rst_info *       g_rstInfo;
boolean          g_OTAInProgress = false;
uint16_t         g_Max_PWM = C_MAX_PWM;
double           g_LED_brightness_factor = 1.15;
boolean          g_timer_reset = false;

WiFiUDP          UDP;                             // Create an instance of the WiFiUDP class
IPAddress        g_timeServerIP;                  // NTP server ip-address
byte             g_NTPBuffer[C_NTP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets

ESP8266WebServer webServer(80);

Ticker           g_LED_ticker;

ESP8266Logger    logger;
char             g_logStr[255];
int              g_logSer;
int              g_logWifi;

char *           g_skullsEyesHTML =
//    <meta http-equiv='refresh' content='5'/>\

"<html>\
  <head>\
    <title>ESP8266 Skulls Eyes</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Skulls Eyes</h1>\
    <form action=\"http://%s.local/skullseyes\" method=\"get\">\
      <table>\
        <tbody>\
          <tr>\
            <th>\
              <label>Steps</label>\
            </th>\
            <td>\
              <input id=\"Steps\" name=\"Steps\" value=%d>\
            </td>\
          </tr>\
          <tr>\
            <th>\
              <label>Delay</label>\
            </th>\
            <td>\
              <input id=\"Delay\" name=\"Delay\" value=%d>\
            </td>\
          </tr>\
          <tr>\
            <th>\
              <label>Delay_Top</label>\
            </th>\
            <td>\
              <input id=\"Delay_Top\" name=\"Delay_Top\" value=%d>\
            </td>\
          </tr>\
          <tr>\
            <th>\
              <label>Delay_Bottom</label>\
            </th>\
            <td>\
              <input id=\"Delay_Bottom\" name=\"Delay_Bottom\" value=%d>\
            </td>\
          </tr>\
          <tr>\
            <th>\
              <label>Dimmer %</label>\
            </th>\
            <td>\
              <input id=\"Dimmer\" name=\"Dimmer\" value=%d>\
            </td>\
          </tr>\
          <tr>\
            <th>\
              <label>Control</label>\ 
            </th>\
            <td>\
              <fieldset>\
                <input type=\"radio\" name=\"Control\" id=\"on\" value=\"On\" %s>\
                <label for\"on\"> On</label><br>\
                <input type=\"radio\" name=\"Control\" id=\"off\" value=\"Off\" %s>\
                <label for\"off\"> Off</label><br>\
                <input type=\"radio\" name=\"Control\" id=\"fade\" value=\"Fade\" %s>\
                <label for\"fade\"> Fade</label><br>\
              </fieldset>\
            </td>\
          </tr>\
          <tr>\
            <th>\
              <label>Wakeup hour</label>\
            </th>\
            <td>\
              <input id=\"Wakeup\" name=\"Wakeup\" value=%d>\
            </td>\
          </tr>\
          <tr>\
            <th>\
              <label>Stay alive hours</label>\
            </th>\
            <td>\
              <input id=\"Stay_Alive\" name=\"Stay_Alive\" value=%d>\
            </td>\
          </tr>\
          <tr>\
            <th>\
            </th>\
            <td>\
              <button name=\"Button\" type=\"submit\" value=\"Submit\">Submit</button>\
              <button name=\"Button\" type=\"submit\" value=\"Sleep\">Sleep</button>\
            </td>\
          </tr>\
        </tbody>\
      </table>\
    </form>\
  </body>\
</html>";

