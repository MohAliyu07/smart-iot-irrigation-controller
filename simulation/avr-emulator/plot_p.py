import json, numpy as np, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False})
O='/mnt/user-data/outputs/'
T=json.load(open('timeline.json')); log=T['log']
t=np.array([a for a,b in log]+[18]); v=np.array([b for a,b in log]+[log[-1][1]])
names=['Pump (D1, PB0)','Valve 1 (D2, PB1)','Valve 2 (D3, PB2)','Valve 3 (D4, PB3)','Valve 4 (D5, PB4)','Alarm (D6, PB5)']
cols=['#1f4e9e','#2ca02c','#2ca02c','#2ca02c','#2ca02c','#d62728']
ev=[(2,'RV1 dry\n(70%)'),(4,'RV3 dry\n(70%)'),(6,'Humidity\n92%'),(8,'Humidity\n60%'),(10,'Manual\nbutton ON'),(12,'Button OFF\ntank EMPTY'),(14,'Tank\nrefilled'),(16,'All soil\nwet')]
fig,ax=plt.subplots(figsize=(13,6))
for i in range(6):
    bit=(v>>i)&1; y=5-i
    ax.fill_between(t,y+0.1,y+0.1+0.75*bit,step='post',color=cols[i],alpha=.85); ax.step(t,y+0.1+0.75*bit,where='post',c=cols[i],lw=1)
ax.set_yticks([5-i+0.47 for i in range(6)]); ax.set_yticklabels(names)
lat=[]
for te,lab in ev:
    ax.axvline(te,c='gray',ls=':',lw=1); ax.text(te+0.05,6.05,lab,fontsize=8,va='bottom')
    prev=[b for a,b in log if a<te][-1]; ch=[a for a,b in log if a>=te and b!=prev]
    if ch: lat.append((lab.replace('\n',' '),(ch[0]-te)*1000))
ax.set_xlim(0,18); ax.set_ylim(-0.1,7); ax.set_xlabel('Time (s)')
ax.set_title('Proteus ATmega328P firmware – output response to test inputs (irrigation_avr.hex)',loc='left',fontweight='bold')
fig.tight_layout(); fig.savefig(O+'Proteus_1_Output_Timeline.png',dpi=160)
S=json.load(open('sweep.json'))
fig,ax=plt.subplots(1,2,figsize=(12,4.2))
m=np.array(S['moist']); ax[0].step(m[:,0],(m[:,2].astype(int)>>1)&1,where='post',c='#2ca02c',lw=2.2,label='Valve 1')
ax[0].step(m[:,0],(m[:,2].astype(int))&1,where='post',c='#1f4e9e',lw=1.2,ls='--',label='Pump')
ax[0].axvline(40,c='r',ls='--',lw=1); ax[0].text(40.3,0.5,'switches at 40.0%\n(ADC 102 / 255)',color='r',fontsize=9)
ax[0].set_yticks([0,1]); ax[0].set_yticklabels(['OFF','ON']); ax[0].set_xlabel('RV1 soil reading (% dry)'); ax[0].legend(loc='upper left')
ax[0].set_title('Dry threshold: zone 1 valve vs soil reading',loc='left',fontweight='bold')
h=np.array(S['hum']); ax[1].step(h[:,0],(h[:,1]&1),where='post',c='#1f4e9e',lw=2.2,label='Pump (all zones dry)')
ax[1].axvline(90,c='r',ls='--',lw=1); ax[1].text(90.3,0.5,'rain lock-out at 90%',color='r',fontsize=9)
ax[1].set_yticks([0,1]); ax[1].set_yticklabels(['OFF','ON']); ax[1].set_xlabel('DHT11 humidity (%)'); ax[1].legend(loc='upper right')
ax[1].set_title('Humidity lock-out: pump vs humidity',loc='left',fontweight='bold')
fig.tight_layout(); fig.savefig(O+'Proteus_2_Thresholds.png',dpi=160)
fig,ax=plt.subplots(figsize=(10,4.2))
labs=[a for a,b in lat]; vals=[b for a,b in lat]
bars=ax.barh(labs[::-1],vals[::-1],color='#1f4e9e'); ax.axvline(525,c='r',ls='--',lw=1); ax.text(530,len(labs)-0.6,'max = 1 loop (525 ms)',color='r',fontsize=9)
for b,x in zip(bars,vals[::-1]): ax.text(x+8,b.get_y()+b.get_height()/2,f'{x:.0f} ms',va='center',fontsize=9)
ax.set_xlabel('Time from input change to output change (ms)'); ax.set_xlim(0,650)
ax.set_title('Response time per event (Proteus firmware)',loc='left',fontweight='bold')
fig.tight_layout(); fig.savefig(O+'Proteus_3_Response_Time.png',dpi=160)
json.dump(lat,open('lat.json','w')); print(lat)
