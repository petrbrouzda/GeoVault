#ifndef __WEB_SERVER___H_
#define __WEB_SERVER___H_

/*

Zjednodušení kombinace WiFi AP + DNS server + webserver pro zařízení, která vystavují konfigurační interface přes web.

Pouzivaji se tyto dve knihovny:
- https://github.com/ESP32Async/ESPAsyncWebServer
- https://github.com/ESP32Async/AsyncTCP 
*/

#include <Arduino.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "src/logging/AsyncLogger.h"

#define EASYWEBSERVER_VERSION "2.0"

// takovahle funkce musi byt v INO kodu aplikace
void userRoutes( AsyncWebServer * server );


enum EwsMode {
    EWS_NONE,
    EWS_CLIENT,
    EWS_AP,
    EWS_AP_CLIENT,
};


#define EWS_MAX_IP_LENGTH 50

class EasyWebServer
{
    public:
        EasyWebServer( AsyncLogger * logger );

        /** spusti wifi v AP rezimu */
        void startApAndWebserver();
        /** spusti wifi v AP+STA rezimu */
        void startApAndStaWebserver();
        /** predpoklada, ze wifi uz bezi */
        void startWebserverClientMode();

        /** 
         * Odbavuje DNS a WiFi status pozadavky, musi byt volano z loop()
         */
        void process();

        const char * getQueryParamAsString( AsyncWebServerRequest *request, const char * paramName, const char * defaultValue );
        long getQueryParamAsLong( AsyncWebServerRequest *request, const char * paramName, long defaultValue );

        const char* HTML_UTF8 = "text/html; charset=utf-8";

        bool staConnected();
        const char * staIp();

    private:
        AsyncLogger * logger;
        AsyncWebServer * server;
        DNSServer dnsServer;

        EwsMode mode;

        int wifiStatus = -1;
        bool wifiConnected = false;
        char clientIp[EWS_MAX_IP_LENGTH];

        /** start webserveru */
        void begin();
};


#endif
