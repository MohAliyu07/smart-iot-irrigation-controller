#include "hal.h"
double g_t=0; float g_temp=27, g_hum=60; int pinState[64]; double pwmDuty=0; SerialT Serial;
#include "fw_core.inc"
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
void ledcWrite(int pin,int v){ if(pin==PIN_PUMP_EN){ if(v==0&&pwmDuty>0&&tOff<0&&g_t>59) tOff=g_t; pwmDuty=v/1023.0; } }
FILE *fast=nullptr;
void plantAdvance(double dt){
  g_t+=dt;
  int nOpen=0; for(int z=0;z<4;z++) nOpen+=pinState[vp[z]];
  double load = tankEmpty?0.15:(0.90+0.15*nOpen);          // running current (A) set by water load
  double target = jam?0:pwmDuty;
  spd += (target-spd)*dt/TAU; if(spd<0)spd=0;
  Ipump = pwmDuty>0 ? load*spd + Vin*std::max(0.0,pwmDuty-spd)/R : 0;   // surge while motor catches up
  Vin = supplyNom - 0.12*Ipump;
  bool flowing = pwmDuty>0.5 && !tankEmpty && !jam;
  for(int z=0;z<4;z++){
    if(flowing && pinState[vp[z]]) moist[z]+=waterRate/3600.0*dt;
    else moist[z]-=dryRate[z]/3600.0*dt;
    moist[z]=std::max(0.0,std::min(100.0,moist[z]));
  }
  if(fast) fprintf(fast,"%.4f,%.4f,%.4f,%.4f,%.4f\n",g_t,pwmDuty,spd,Ipump,Vin);
}
double plantMilliVolts(int p){
  if(p>=1&&p<=4){ double raw=MOIST_RAW_DRY-(MOIST_RAW_DRY-MOIST_RAW_WET)*moist[p-1]/100.0; return raw/4095.0*3300.0; }
  if(p==PIN_VIN_SENSE) return Vin/VIN_DIV_RATIO*1000.0;
  if(p==PIN_PUMP_ISENSE) return Ipump*ISENSE_V_PER_A*1000.0;
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
void reset(){ tOff=-1; g_t=0; spd=0; pwmDuty=0; tankEmpty=jam=false; supplyNom=12.1; for(auto&p:pinState)p=0;
  outputsBegin(); sensorsBegin(); zonesBegin(); zonesClearFault(); zonesSetMode(MODE_AUTO); }
int main(){
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
  return 0;
}
