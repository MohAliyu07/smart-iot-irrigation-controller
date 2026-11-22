#pragma once
#include <functional>
#include <cstdarg>
struct BParam{ int asInt() const {return 0;} };
struct BlynkC{ bool connected(){return false;} void run(){} void config(const char*){} template<class...A> void virtualWrite(A...){} template<class...A> void syncVirtual(A...){} }; static BlynkC Blynk;
struct BlynkTimer{ void setInterval(unsigned long, std::function<void()>){} void run(){} };
enum {V0,V1,V2,V3,V4,V5,V6,V7,V8,V9,V10,V11,V12,V13,V14,V15,V16};
#define BLYNK_WRITE(p) void bw_##p(const BParam& param)
#define BLYNK_CONNECTED() void b_conn()
