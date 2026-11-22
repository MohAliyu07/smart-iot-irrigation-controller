#pragma once
#define WL_CONNECTED 3
struct WiFiC{ void begin(const char*,const char*){} int status(){return 0;} }; static WiFiC WiFi;
