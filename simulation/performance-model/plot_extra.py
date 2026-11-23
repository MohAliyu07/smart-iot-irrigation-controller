import pandas as pd, numpy as np, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
plt.rcParams.update({'font.size':10,'axes.grid':True,'grid.alpha':.3,'axes.spines.top':False,'axes.spines.right':False})
O='/mnt/user-data/outputs/revb/RevB_'; B='#1f77b4'; C=['#1f77b4','#ff7f0e','#2ca02c','#d62728']
def edge(d,col,up=True):
    x=(d[col]>0).astype(int).diff().fillna(0); r=d[x==(1 if up else -1)]; return r.t.iloc[0]
# ---- Perf 6: sequencing ----
s=pd.read_csv('x_sequence.csv'); fig,ax=plt.subplots(1,3,figsize=(13,4))
tv=edge(s,'v4'); tp=edge(s,'duty'); a=pd.concat([pd.DataFrame({'t':[-0.3],'v4':[0],'duty':[0]}),s[s.t<=1.0]])
ax[0].plot(a.t,a.v4*100,drawstyle='steps-post',c=C[2],lw=1.8,label='Valve 4 (open = 100)'); ax[0].plot(a.t,a.duty*100,c=B,lw=1.8,label='Pump PWM duty (%)')
ax[0].axvline(tv,c='gray',ls=':',lw=1); ax[0].axvline(tp,c='r',lw=1)
ax[0].text(tv-0.01,55,f'Valve opens\nat {tv:.2f} s',fontsize=8,ha='right',color='gray')
ax[0].text(-0.28,6,f'Pump starts\nat {tp:.2f} s\n(+{(tp-tv)*1000:.0f} ms), then\n500 ms soft start',fontsize=8.5,color='r',fontweight='bold')
ax[0].set_ylim(0,110); ax[0].set_xlim(-0.3,1.0); ax[0].set_ylabel('Valve state / PWM duty (%)'); ax[0].set_title('Zone starts: valve first, then pump',loc='left',fontweight='bold',fontsize=10); ax[0].legend(loc='lower right',fontsize=8)
tp2=edge(s[s.t>5],'duty',False); tv2=edge(s[s.t>5],'v4',False); a=s[(s.t>=tp2-0.2)&(s.t<=tp2+0.3)]
ax[1].plot(a.t,a.v4*100,drawstyle='steps-post',c=C[2],lw=1.8,label='Valve 4 (open = 100)'); ax[1].plot(a.t,a.duty*100,drawstyle='steps-post',c=B,lw=1.8,label='Pump PWM duty (%)')
ax[1].axvline(tp2,c='r',lw=1); ax[1].axvline(tv2,c='gray',ls=':',lw=1)
ax[1].text(tp2-0.01,55,f'Pump off\nat {tp2:.2f} s',fontsize=8.5,ha='right',color='r',fontweight='bold')
ax[1].text(tv2+0.01,55,f'Valve closes at {tv2:.2f} s\n(+{(tv2-tp2)*1000:.0f} ms)',fontsize=8,color='gray')
ax[1].set_ylim(0,110); ax[1].set_title('Zone stops: pump first, then valve',loc='left',fontweight='bold',fontsize=10); ax[1].legend(loc='lower left',fontsize=8)
t=pd.read_csv('x_twovalves.csv')
for z in range(4): ax[2].fill_between(t.t,z+0.1,z+0.1+0.8*t[f'v{z+1}'],step='post',color=C[z],alpha=.85)
ax[2].fill_between(t.t,4.1,4.1+0.8*(t.duty>0.5),step='post',color='#444')
ax[2].set_yticks([.5,1.5,2.5,3.5,4.5]); ax[2].set_yticklabels(['Valve 1','Valve 2','Valve 3','Valve 4','Pump']); ax[2].grid(False)
n=(t.v1+t.v2+t.v3+t.v4).max()
for te,lab in [(edge(t,'v3'),'Zone 1 wet,\nzone 3 takes over'),(edge(t,'v4'),'Zone 2 wet,\nzone 4 takes over')]:
    ax[2].axvline(te,c='gray',ls=':',lw=1); ax[2].text(te+0.8,0.25 if lab[5]=='1' else 1.25,lab,fontsize=8,color='gray')
ax[2].text(1.5,3.45,f'All four zones dry:\nnever more than {n:.0f} valves open',fontsize=8.5,color='r',fontweight='bold')
ax[2].set_xlim(0,60); ax[2].set_title('Four dry zones, two-valve limit',loc='left',fontweight='bold',fontsize=10)
for x in ax: x.set_xlabel('Time (s)')
fig.suptitle('Valve and pump sequencing',x=0.01,ha='left',fontweight='bold'); fig.tight_layout(); fig.savefig(O+'Perf_6_Sequencing.png',dpi=160)
# ---- Perf 7: new protection ----
fig,ax=plt.subplots(1,3,figsize=(13,4)); rows=[]
for i,(f,tev,ev,title,win,xt) in enumerate([('unplug',57.0,'Sensor unplugged','Soil sensor unplugged (at 57 s)',(54.2,61.5),None),
                                            ('mode',60.0,'Mode set to OFF','Mode switched to OFF (at 60 s)',(59.8,60.6),None),
                                            ('timeout',None,None,'Soil never gets wet (20-minute cap)',(1198,1202),None)]):
    d=pd.read_csv(f'x_{f}.csv'); toff=edge(d,'duty',False); d=d[(d.t>=win[0])&(d.t<=win[1])]
    ax[i].plot(d.t,d.I,c=B,lw=1.8); ax[i].set_ylim(0,3.2); ax[i].axhline(2.5,c='r',ls='--',lw=1,label='overcurrent limit 2.5 A')
    ax[i].axvline(toff,c='r',lw=1)
    if tev is not None:
        ax[i].axvline(tev,c='gray',ls=':',lw=1); ax[i].text(tev-(win[1]-win[0])*0.012,1.76,f'{ev}\nat {tev:.1f} s',fontsize=8,ha='right',color='gray')
        dt=toff-tev; lab=(f'Pump off at {toff:.2f} s\n(+{dt*1000:.0f} ms)' if dt>0.01 else f'Pump off at {toff:.2f} s\n(straight away)') if dt<1 else f'Pump off at {toff:.2f} s\n(+{dt:.2f} s, at the\nnext sensor read)'
        rows.append((title,f'{dt*1000:.0f} ms'))
    else:
        lab=f'Pump off at {toff:.1f} s\n= 20 min after the\nvalve opened'; ax[i].text(win[0]+0.1,1.76,'Watering since 0.2 s,\nmoisture still below 35 %',fontsize=8,color='gray'); rows.append((title,f'{toff:.1f} s'))
    ha='left'; xo=toff+(win[1]-win[0])*0.025
    if f=='unplug': ha='right'; xo=toff-(win[1]-win[0])*0.025; 
    ax[i].text(xo,2.0 if f!='unplug' else 0.35,lab,fontsize=8.5,color='r',fontweight='bold',ha=ha)
    ax[i].set_title(title,loc='left',fontweight='bold',fontsize=10); ax[i].set_xlabel('Time (s)'); ax[i].set_ylabel('Pump current (A)'); ax[i].legend(loc='upper right',fontsize=8)
fig.suptitle('Protection added in the Rev B firmware',x=0.01,ha='left',fontweight='bold'); fig.tight_layout(); fig.savefig(O+'Perf_7_New_Protection.png',dpi=160)
print(tv,tp,tp2,tv2,n,rows)
