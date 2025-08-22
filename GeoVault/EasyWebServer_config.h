#ifndef __WEBSERVER___CONFIG__
#define __WEBSERVER___CONFIG__

#define SSID "GeoVault"
#define PASSWORD "SuperTajneHeslo2"

#define CLIENT_SSID "--"
#define CLIENT_PASSWORD "--"

// konfigurace IP
IPAddress local_ip(192,168,10,1);
IPAddress gateway(192,168,10,1);
IPAddress subnet(255,255,255,0);

#define MY_NAME "192.168.10.1"

// na jakou adresu ma presmerovavat captive portal?
#define CAPTIVE_PORTAL_REDIRECT_REL "/"

// pokud je dementni ESP32C3-micro s kondenzatorem misto PCB anteny, nastav true = sniz vysilaci vykon 
#define FIX_POWER_FOR_ESP32C3MICRO true

#define WEB_PORT 80

#endif