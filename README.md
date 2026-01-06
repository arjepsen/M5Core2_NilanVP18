For wifi: create a "secrets.h" file in the include directory, with content like this:

#ifndef SECRETS_H
#define SECRETS_H

const char* WIFI_STA_DEFAULT_SSID = "YOUR_SSID";
const char* WIFI_STA_DEFAULT_PASS = "YOUR_PASSWORD";

#endif 