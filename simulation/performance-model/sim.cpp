#include "hal.h"
bool g_revA=false; static unsigned lcg=12345; static double rnd(){ lcg=lcg*1664525u+1013904223u; return (lcg>>8)/16777216.0; }
FILE *rampf=nullptr; double Ipump_pub=0;
void simRampLog(float duty,float rd){ if(rampf && g_revA) fprintf(rampf,"%.4f,%.3f,%.4f,%.4f\n",g_t,duty,Ipump_pub,rd); }
bool recAll=false;
double g_t=0; float g_temp=27, g_hum=60; int pinState[64]; double pwmDuty=0; SerialT Serial;
#ifdef REVA
#include "fw_core.inc"      // old board + old code, replayed with g_revA = true
#else
#include "config.h"
#include "sensors.h"
#include "outputs.h"
#include "zones.h"     // the real Rev B firmware sources are compiled and linked in
#endif
// ---------------- plant model ----------------
double moist[4]={70,62,55,48};
double dryRate[4]={6,9,12,15};   // % per hour lost
double waterRate=120;            // % per hour gained while watered
double spd=0;                    // motor speed 0..1
double Ipump=0, Vin=12.1;
bool tankEmpty=false, jam=false; double supplyNom=12.1;
const double R=4.0, TAU=0.15;
static const int vp[4]={PIN_VALVE1,PIN_VALVE2,PIN_VALVE3,PIN_VALVE4};
double tOff=-1;
void ledcWrite(int pin,int v){ if(pin==PIN_PUMP_EN){ if(v==0&&pwmDuty>0&&tOff<0&&(g_t>59||recAll)) tOff=g_t; pwmDuty=v/1023.0; } }
FILE *fast=nullptr; FILE *fast2=nullptr; double fast2From=0; bool unplug[4]={false,false,false,false};
void plantAdvance(double dt){
  g_t+=dt;
  int nOpen=0; for(int z=0;z<4;z++) nOpen+=pinState[vp[z]];
  double load = tankEmpty?0.15:(0.90+0.15*nOpen);          // running current (A) set by water load
  double target = jam?0:pwmDuty;
  spd += (target-spd)*dt/TAU; if(spd<0)spd=0;
  Ipump = pwmDuty>0 ? load*spd + Vin*std::max(0.0,pwmDuty-spd)/R : 0;   // surge while motor catches up
  Vin = supplyNom - 0.12*Ipump; Ipump_pub=Ipump;
  bool flowing = pwmDuty>0.5 && !tankEmpty && !jam;
  for(int z=0;z<4;z++){
    if(flowing && pinState[vp[z]]) moist[z]+=waterRate/3600.0*dt;
    else moist[z]-=dryRate[z]/3600.0*dt;
    moist[z]=std::max(0.0,std::min(100.0,moist[z]));
  }
  if(fast) fprintf(fast,"%.4f,%.4f,%.4f,%.4f,%.4f\n",g_t,pwmDuty,spd,Ipump,Vin);
  if(fast2 && g_t>=fast2From) fprintf(fast2,"%.3f,%.4f,%.4f,%d,%d,%d,%d,%d,%d\n",g_t,pwmDuty,Ipump,pinState[vp[0]],pinState[vp[1]],pinState[vp[2]],pinState[vp[3]],(int)zonesFault(),(int)zonesMode());
}
double plantMilliVolts(int p){
  if(p>=1&&p<=4){ if(unplug[p-1]) return 3300.0; double raw=MOIST_RAW_DRY-(MOIST_RAW_DRY-MOIST_RAW_WET)*moist[p-1]/100.0; return raw/4095.0*3300.0; }
  if(p==PIN_VIN_SENSE) return Vin/VIN_DIV_RATIO*1000.0;
  if(p==PIN_PUMP_ISENSE){
    // Shunt is in the MOSFET source: it carries the motor current only during the PWM on-time.
    double D=pwmDuty, I=Ipump;
    if(D<=0) return 0; if(D>=0.999) return I*ISENSE_V_PER_A*1000.0;
    if(g_revA) return (rnd()<D ? I : 0.0)*ISENSE_V_PER_A*1000.0;          // Rev A: no filter, one sample lands on- or off-time
    const double T=1.0/20000, tau=1000.0*100e-9;                           // Rev B: R36 1k x C17 100nF
    double ripple = I*D*(1-D)*T/tau;                                       // peak-to-peak after the RC filter
    double v=(I*D + ripple*(rnd()-0.5));
    if(rampf) fprintf(rampf,"%.4f,%.3f,%.4f,%.4f\n",g_t,D,I,v);
    return v*ISENSE_V_PER_A*1000.0;
  }
  return 0;
}
SensorData data;
FILE *lg;
void logRow(){
  fprintf(lg,"%.1f,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%.3f,%.3f,%d,%d,%d,%d,%d,%d\n",g_t,moist[0],moist[1],moist[2],moist[3],
    pinState[PIN_VALVE1],pinState[PIN_VALVE2],pinState[PIN_VALVE3],pinState[PIN_VALVE4],pwmDuty>0.5,Ipump,Vin,
    data.moisturePct[0],data.moisturePct[1],data.moisturePct[2],data.moisturePct[3],(int)zonesFault(),valvesOpenCount());
}
// run: duration seconds, event callback
template<class F> void run(const char*name,double dur,double logEvery,F ev){
  lg=fopen(name,"w"); fprintf(lg,"t,m1,m2,m3,m4,v1,v2,v3,v4,pump,I,Vin,r1,r2,r3,r4,fault,open\n");
  double nextSensor=0,nextLog=0;
  while(g_t<dur){
    ev();
    if(g_t>=nextSensor){ sensorsRead(data); zonesUpdate(data); nextSensor+=SENSOR_PERIOD_MS/1000.0; }
    zonesSafetyTick();
    if(g_t>=nextLog){ logRow(); nextLog+=logEvery; }
    for(int k=0;k<100;k++) plantAdvance(0.001);
  }
  fclose(lg);
  FILE*tf=fopen("trips.csv","a"); fprintf(tf,"%s,%.3f\n",name,tOff); fclose(tf);
}
void reset(){
#ifndef REVA
  allOff();   // the firmware keeps its pump state between scenarios
#endif
  for(auto&u:unplug)u=false; waterRate=120; g_revA=false; lcg=12345; tOff=-1; g_t=0; spd=0; pwmDuty=0; tankEmpty=jam=false; supplyNom=12.1; for(auto&p:pinState)p=0;
  outputsBegin(); sensorsBegin(); zonesBegin(); zonesClearFault(); zonesSetMode(MODE_AUTO); }
int main(){
#ifndef REVA
  // 1) 8-hour automatic run
  reset(); run("auto8h.csv",8*3600,10,[]{});
  // 2) dry-run: tank empties at t=60 s while zone 4 watering
  reset(); double m0[4]={70,70,70,30}; for(int i=0;i<4;i++)moist[i]=m0[i];
  fast=fopen("dryrun_fast.csv","w"); fprintf(fast,"t,duty,spd,I,Vin\n"); run("dryrun.csv",120,0.1,[]{ if(g_t>=59.9995) tankEmpty=true; }); fclose(fast); fast=nullptr;
  // 3) overcurrent: jam at t=60
  reset(); for(int i=0;i<4;i++)moist[i]=m0[i];
  fast=fopen("jam_fast.csv","w"); fprintf(fast,"t,duty,spd,I,Vin\n"); run("jam.csv",120,0.1,[]{ if(g_t>=59.9995) jam=true; }); fclose(fast); fast=nullptr;
  // 4) supply sag at t=60
  reset(); for(int i=0;i<4;i++)moist[i]=m0[i];
  fast=fopen("lowvin_fast.csv","w"); fprintf(fast,"t,duty,spd,I,Vin\n"); run("lowvin.csv",120,0.1,[]{ if(g_t>=59.9995) supplyNom=10.2; }); fclose(fast); fast=nullptr;
  // 5) soft start vs hard start current (1 ms resolution)
  reset(); valveSet(0,true); fast=fopen("soft.csv","w"); fprintf(fast,"t,duty,spd,I,Vin\n"); pumpStart(); delay(700); fclose(fast); fast=nullptr; pumpStop();
  reset(); valveSet(0,true); fast=fopen("hard.csv","w"); fprintf(fast,"t,duty,spd,I,Vin\n"); ledcWrite(PIN_PUMP_EN,1023); delay(1200); fclose(fast); fast=nullptr;

  // ---- extra scenarios for the Rev B rewrite (1 ms logs: t,duty,I,v1..v4,fault,mode) ----
  auto setM=[](double a,double b,double c2,double d){ moist[0]=a;moist[1]=b;moist[2]=c2;moist[3]=d; };
  auto open2=[](const char*n,double from){ fast2=fopen(n,"w"); fast2From=from; fprintf(fast2,"t,duty,I,v1,v2,v3,v4,fault,mode\n"); };
  // 8) start and stop sequence of one zone (zone 4 dry at t=0, wet at t=20)
  reset(); recAll=true; setM(70,70,70,30); open2("x_sequence.csv",0); run("x_sequence_log.csv",35,0.1,[]{ if(g_t>=19.9995&&g_t<20.2) moist[3]=62; }); fclose(fast2); fast2=nullptr;
  // 9) four dry zones: only two valves at a time, third takes over when one finishes
  reset(); recAll=true; setM(30,30,30,30); open2("x_twovalves.csv",0); run("x_twovalves_log.csv",60,0.1,[]{ if(g_t>=27.9995&&g_t<28.2) moist[0]=62; if(g_t>=47.9995&&g_t<48.2) moist[1]=62; }); fclose(fast2); fast2=nullptr;
  // 10) soil sensor unplugged at t=57 s while its zone is watering
  reset(); recAll=true; setM(70,70,70,30); open2("x_unplug.csv",50); run("x_unplug_log.csv",75,0.1,[]{ if(g_t>=56.9995) unplug[3]=true; }); fclose(fast2); fast2=nullptr;
  // 11) mode switched to OFF at t=60 s while watering
  reset(); recAll=true; setM(70,70,70,30); open2("x_mode.csv",55); run("x_mode_log.csv",70,0.1,[]{ if(g_t>=59.9995&&zonesMode()!=MODE_OFF) zonesSetMode(MODE_OFF); }); fclose(fast2); fast2=nullptr;
  // 13) supply drop at different points of the 10 s sensor period: trip delay must not depend on the phase
  { FILE*sw=fopen("x_vinsweep.csv","w"); fprintf(sw,"t_event,delay_ms\n");
    static double te; for(double e: {60.0,61.3,62.7,64.1,65.5,66.9,68.3,69.7}){ te=e; reset(); recAll=false; setM(70,70,70,30);
      run("x_vinsweep_log.csv",e+12,0.1,[]{ if(g_t>=te-0.0005) supplyNom=10.2; });
      fprintf(sw,"%.1f,%.0f\n",e,(tOff-e)*1000); } fclose(sw); }
  // 12) soil never gets wet (blocked pipe or bad sensor reading dry): 20-minute cap
  reset(); recAll=true; waterRate=0; setM(70,70,70,30); open2("x_timeout.csv",1190); run("x_timeout_log.csv",1215,0.1,[]{}); fclose(fast2); fast2=nullptr; recAll=false;
#endif
  // 6) what the MCU reads during a normal soft start: Rev A vs Rev B
  for(int rv=0; rv<2; rv++){
#ifdef REVA
    if(rv==1) continue;
#else
    if(rv==0) continue;
#endif
    reset(); g_revA=(rv==0); valveSet(0,true); rampf=fopen(rv==0?"ramp_revA.csv":"ramp_revB.csv","w"); fprintf(rampf,"t,duty,I,reading\n"); pumpStart(); fclose(rampf); rampf=nullptr; pumpStop(); }
  // 7) stalled pump at start-up: Rev A vs Rev B
  for(int rv=0; rv<2; rv++){
#ifdef REVA
    if(rv==1) continue;
#else
    if(rv==0) continue;
#endif
    reset(); g_revA=(rv==0); recAll=true; jam=true; double m1[4]={70,70,70,30}; for(int i=0;i<4;i++)moist[i]=m1[i];
    fast=fopen(rv==0?"stall_revA_fast.csv":"stall_revB_fast.csv","w"); fprintf(fast,"t,duty,spd,I,Vin\n"); run(rv==0?"stall_revA.csv":"stall_revB.csv",3,0.1,[]{}); fclose(fast); fast=nullptr; recAll=false; }
  g_revA=false;
  return 0;
}
