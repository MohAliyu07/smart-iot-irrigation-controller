#pragma once
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <algorithm>
using std::isnan;
typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define ADC_11db 3
extern double g_t;            // seconds
extern float g_temp, g_hum;
extern int pinState[64];
extern double pwmDuty;        // 0..1
void plantAdvance(double dt);
double plantMilliVolts(int pin);
inline unsigned long millis(){ return (unsigned long)(g_t*1000.0+0.5); }
inline unsigned long micros(){ return (unsigned long)(g_t*1e6); }
inline void delay(unsigned long ms){ for(unsigned long i=0;i<ms;i++) plantAdvance(0.001); }
inline void delayMicroseconds(unsigned long){}
inline void pinMode(int,int){}
inline void digitalWrite(int p,int v){ pinState[p]=v; }
inline int digitalRead(int p){ return pinState[p]; }
inline void analogReadResolution(int){}
inline void analogSetAttenuation(int){}
inline uint32_t analogReadMilliVolts(int p){ return (uint32_t)std::max(0.0,plantMilliVolts(p)); }
inline int analogRead(int p){ return (int)std::min(4095.0, plantMilliVolts(p)/3300.0*4095.0); }
inline void ledcAttach(int,int,int){}
void ledcWrite(int pin,int v);
inline long map(long x,long a,long b,long c,long d){ return (x-a)*(d-c)/(b-a)+c; }
template<class T,class A,class B> T constrain(T x,A a,B b){ return x<(T)a?(T)a:(x>(T)b?(T)b:x); }
struct SerialT{ void printf(const char*,...){} void println(const char*){} void begin(int){} };
extern SerialT Serial;
inline void noInterrupts(){} inline void interrupts(){}
