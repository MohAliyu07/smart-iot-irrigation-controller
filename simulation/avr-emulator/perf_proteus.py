import emu, json
F=8_000_000
class Dyn(emu.Env):
    def __init__(s,sched):
        super().__init__([0,0,0,0]); s.sched=sched; s.cpu=None; s.trace=[]
    def cur(s,cyc):
        st={}
        for t,ch in s.sched:
            if cyc>=t*F: st.update(ch)
        return st
    def adc(s,ch): return s.cur(s.cpu.cyc)['adc'][ch]
    def start_wave(s,t0):
        st=s.cur(t0); s.hum=st.get('hum',60); s.dht=st.get('dht',True); super().start_wave(t0)
    def pind(s,cpu):
        st=s.cur(cpu.cyc); s.manual=st.get('manual',False); s.tank=st.get('tank',False); return super().pind(cpu)
def run(sched,secs):
    e=Dyn(sched); c=emu.AVR(e); e.cpu=c
    while c.cyc<secs*F: c.step()
    return [(cy/F,v) for cy,v in e.portb_log], e
pct=lambda p: round(p*255/100)
WET=pct(20); DRY=pct(70)
base={'adc':[WET]*4,'hum':60,'manual':False,'tank':False}
sched=[(0,dict(base)),
 (2,{'adc':[DRY,WET,WET,WET]}),
 (4,{'adc':[DRY,WET,DRY,WET]}),
 (6,{'hum':92}),
 (8,{'hum':60}),
 (10,{'manual':True}),
 (12,{'manual':False,'tank':True}),
 (14,{'tank':False}),
 (16,{'adc':[WET]*4})]
log,_=run(sched,18)
json.dump({'sched':[(t,c) for t,c in sched],'log':log},open('timeline.json','w'))
print(log)
