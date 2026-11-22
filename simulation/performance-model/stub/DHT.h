#pragma once
#include "../hal.h"
#define DHT11 11
struct DHT { DHT(int,int){} void begin(){} float readTemperature(){ return g_temp; } float readHumidity(){ return g_hum; } };
