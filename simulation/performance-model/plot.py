import pandas as pd, numpy as np, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
plt.rcParams.update({'font.size':10,'axes.grid':True,'grid.alpha':.3,'axes.spines.top':False,'axes.spines.right':False})
C=['#1f77b4','#ff7f0e','#2ca02c','#d62728']; O='/mnt/user-data/outputs/revb/RevB_'
a=pd.read_csv('auto8h.csv'); h=a.t/3600
fig,ax=plt.subplots(3,1,figsize=(11,8.5),sharex=True,gridspec_kw={'height_ratios':[3,1.6,1.4]})
for z in range(4): ax[0].plot(h,a[f'm{z+1}'],c=C[z],lw=1.6,label=f'Zone {z+1}')
ax[0].axhline(35,c='k',ls='--',lw=1);ax[0].axhline(60,c='k',ls=':',lw=1)
ax[0].text(8.02,35,'dry 35% (start)',va='center',fontsize=8);ax[0].text(8.02,60,'wet 60% (stop)',va='center',fontsize=8)
ax[0].set_ylabel('Soil moisture (%)');ax[0].set_ylim(20,80);ax[0].legend(ncol=4,loc='upper right');ax[0].set_title('Automatic watering over 8 hours (4 zones)',loc='left',fontweight='bold')
for z in range(4): ax[1].fill_between(h,z+0.1,z+0.1+0.8*a[f'v{z+1}'],step='post',color=C[z],alpha=.85)
ax[1].fill_between(h,4.1,4.1+0.8*a.pump,step='post',color='#444')
ax[1].set_yticks([.5,1.5,2.5,3.5,4.5]);ax[1].set_yticklabels(['Valve 1','Valve 2','Valve 3','Valve 4','Pump']);ax[1].grid(False)
ax[2].plot(h,a.I,c='#6a3d9a',lw=1);ax[2].axhline(2.5,c='r',ls='--',lw=1);ax[2].text(8.02,2.5,'trip 2.5 A',va='center',fontsize=8,color='r')
ax[2].set_ylabel('Pump current (A)');ax[2].set_ylim(0,3);ax[2].set_xlabel('Time (hours)')
fig.tight_layout();fig.savefig(O+'Perf_1_Auto_Watering_8h.png',dpi=160)
TR=dict(pd.read_csv('trips.csv',header=None).values)
# faults
fig,ax=plt.subplots(1,3,figsize=(13,4))
for i,(f,col,th,lab,title,ev) in enumerate([('dryrun','I',0.25,'dry-run limit 0.25 A','Dry run (tank empties at 60 s)','Tank empty'),
                                    ('jam','I',2.5,'overcurrent limit 2.5 A','Pump jam (at 60 s)','Jam'),
                                    ('lowvin','Vin',10.5,'low supply limit 10.5 V','Supply drop to 10.2 V (at 60 s)','Supply drop')]):
    d=pd.read_csv(f+'_fast.csv'); tr=TR[f+'.csv']; d=d[(d.t>=59.8)&(d.t<=60.6)]
    ax[i].plot(d.t,d[col],drawstyle='steps-post' if col=='Vin' else 'default',c='#1f77b4',lw=1.8);ax[i].axhline(th,c='r',ls='--',lw=1,label=lab)
    ax[i].axvline(60,c='gray',ls=':',lw=1);ax[i].axvline(tr,c='r',lw=1)
    if col=='Vin': ax[i].set_ylim(9.5,12.5)
    else: ax[i].set_ylim(0,3.2)
    y0,y1=ax[i].get_ylim(); ax[i].text(59.99,y0+(y1-y0)*0.55,f'{ev} at 60.0 s',fontsize=8,ha='right',color='gray')
    ax[i].text(tr+0.02,y0+(y1-y0)*0.6,f'Pump off at {tr:.2f} s\n(+{(tr-60)*1000:.0f} ms)',fontsize=8.5,color='r',fontweight='bold')
    if col=='Vin':
        sw=pd.read_csv('x_vinsweep.csv'); ax[i].text(0.03,0.015,f'Checked every 100 ms, 2 readings in a row.\n{len(sw)} drop times across the 10 s sensor period:\npump off after {sw.delay_ms.min():.0f} to {sw.delay_ms.max():.0f} ms',transform=ax[i].transAxes,fontsize=7.5,color='#333')
    ax[i].set_title(title,loc='left',fontweight='bold',fontsize=10);ax[i].set_xlabel('Time (s)');ax[i].set_ylabel('Pump current (A)' if col=='I' else 'Supply voltage (V)');ax[i].legend(loc='lower right' if col=='I' else 'upper right',fontsize=8)
fig.suptitle('Fault protection response',x=0.01,ha='left',fontweight='bold');fig.tight_layout();fig.savefig(O+'Perf_2_Fault_Response.png',dpi=160)
# soft start
s=pd.read_csv('soft.csv');hd=pd.read_csv('hard.csv');s.t-=s.t.iloc[0];hd.t-=hd.t.iloc[0]
fig,ax=plt.subplots(figsize=(9,4.5))
ax.plot(hd.t*1000,hd.I,c='#d62728',lw=1.8,label=f'Hard start (full power at once): peak {hd.I.max():.2f} A')
ax.plot(s.t*1000,s.I,c='#2ca02c',lw=1.8,label=f'Soft start (500 ms ramp): peak {s.I.max():.2f} A')
ax.axhline(2.5,c='k',ls='--',lw=1,label='Overcurrent trip 2.5 A');ax.set_xlim(0,700);ax.set_xlabel('Time after pump command (ms)');ax.set_ylabel('Pump current (A)')
ax.set_title('Pump start-up current: soft start vs hard start',loc='left',fontweight='bold');ax.legend();fig.tight_layout();fig.savefig(O+'Perf_3_Soft_Start.png',dpi=160)
# metrics
rows=[]
for z in range(4):
    v=a[f'v{z+1}'];starts=((v.diff()==1)).sum();on=v.sum()*10/60
    rows.append([f'Zone {z+1}',int(starts),round(on,1),round(a[f'm{z+1}'].min(),1),round(a[f'm{z+1}'].max(),1)])
m=pd.DataFrame(rows,columns=['Zone','Watering cycles','Valve open (min)','Min moisture %','Max moisture %'])
tr={f:(TR[f+'.csv']-60)*1000 for f in ['dryrun','jam','lowvin']}
summ=[['Most valves open at once',int(a.open.max()),'limit 2'],['Pump run time in 8 h (min)',round(a.pump.sum()*10/60,1),''],
['Peak running current (A)',round(a.I.max(),2),'limit 2.5'],['Soft-start peak (A)',round(s.I.max(),2),f'hard start {hd.I.max():.2f}'],
['Dry-run trip time (ms)',round(tr['dryrun']),'after tank empty'],['Overcurrent trip time (ms)',round(tr['jam']),'after jam'],
['Low-supply trip time (ms)',round(tr['lowvin']),'checked every 100 ms, 2 readings in a row'],['Low-supply trip, worst of 8 drop times (ms)',int(pd.read_csv('x_vinsweep.csv').delay_ms.max()),'across the 10 s sensor period'],['Faults in normal 8 h run',int(a.fault.max()),'']]
m.to_csv('zones.csv',index=False);pd.DataFrame(summ,columns=['Metric','Value','Note']).to_csv('summary.csv',index=False)
print(m.to_string());print(pd.DataFrame(summ).to_string())
# ---- Rev B additions ----
ra=pd.read_csv('ramp_revA.csv');rb=pd.read_csv('ramp_revB.csv').groupby('t',as_index=False).mean()
fig,ax=plt.subplots(1,2,figsize=(12.5,4.3),sharey=True)
t0=ra.t.iloc[0]-0.01
for a_,d,ttl in [(ax[0],ra,'Rev A: no filter, one ADC sample'),(ax[1],rb,'Rev B: RC filter (1 k + 100 nF), 16 samples, divided by duty')]:
    tt=(d.t-t0)*1000
    a_.plot(tt,d.I,c='#444',lw=2,label='True motor current')
    est=d.reading if 'Rev A' in ttl else d.reading/d.duty
    a_.plot(tt,est,'o',ms=4,c='#d62728' if 'Rev A' in ttl else '#2ca02c',label='What the firmware measures')
    err=(est-d.I).abs(); a_.set_title(ttl,loc='left',fontweight='bold',fontsize=10)
    a_.text(0.98,0.05,f'mean error {err.mean():.2f} A\nworst {err.max():.2f} A',transform=a_.transAxes,ha='right',fontsize=9)
    a_.set_xlabel('Time into soft start (ms)');a_.legend(loc='upper left',fontsize=8)
ax[0].set_ylabel('Pump current (A)');fig.suptitle('Current measurement during the 20 kHz soft-start ramp',x=0.01,ha='left',fontweight='bold');fig.tight_layout();fig.savefig(O+'Perf_4_Current_Sense_Filter.png',dpi=160)
errA=(ra.reading-ra.I).abs();errB=(rb.reading/rb.duty-rb.I).abs()
sa=pd.read_csv('stall_revA_fast.csv');sb=pd.read_csv('stall_revB_fast.csv')
fig,ax=plt.subplots(figsize=(9,4.5))
res={}
for d,c,l in [(sa,'#d62728','Rev A (checks only after the ramp)'),(sb,'#2ca02c','Rev B (checks on every ramp step)')]:
    on=d[d.duty>0];t1=on.t.min();dd=d[(d.t>=t1-0.02)&(d.t<=t1+0.7)]
    res[l[:5]]=(d.I.max(),(on.t.max()-t1)*1000)
    ax.plot((dd.t-t1)*1000,dd.I,c=c,lw=1.8,label=f'{l}: peak {d.I.max():.2f} A, powered {(on.t.max()-t1)*1000:.0f} ms')
ax.axhline(2.5,c='k',ls='--',lw=1,label='Overcurrent limit 2.5 A');ax.set_xlabel('Time after pump command (ms)');ax.set_ylabel('Pump current (A)')
ax.set_title('Stalled pump at start-up',loc='left',fontweight='bold');ax.legend(fontsize=8.5);fig.tight_layout();fig.savefig(O+'Perf_5_Stalled_Start.png',dpi=160)
extra=[['Sense error in soft start, Rev A (A, mean/worst)',f'{errA.mean():.2f} / {errA.max():.2f}','no filter'],
['Sense error in soft start, Rev B (A, mean/worst)',f'{errB.mean():.2f} / {errB.max():.2f}','RC filter + averaging'],
['Stalled start, Rev A: peak A / powered ms',f"{res['Rev A'][0]:.2f} / {res['Rev A'][1]:.0f}",''],
['Stalled start, Rev B: peak A / powered ms',f"{res['Rev B'][0]:.2f} / {res['Rev B'][1]:.0f}",'']]
pd.concat([pd.read_csv('summary.csv'),pd.DataFrame(extra,columns=['Metric','Value','Note'])]).to_csv('summary.csv',index=False)
print(pd.DataFrame(extra).to_string())
