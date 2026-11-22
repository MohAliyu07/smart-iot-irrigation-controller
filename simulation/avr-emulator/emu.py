import fw
W=fw.words
class AVR:
    def __init__(s,env):
        s.r=[0]*32; s.pc=0; s.sp=0; s.cyc=0; s.Z=0; s.C=0; s.mem=[0]*0x900; s.env=env
        s.io={}; s.adc_busy=0
    def rd_io(s,a):  # I/O space address
        if a==0x09: return s.env.pind(s)
        if a==0x3D: return s.sp&0xFF
        if a==0x3E: return s.sp>>8
        return s.io.get(a,0)
    def wr_io(s,a,v):
        if a==0x3D: s.sp=(s.sp&0xFF00)|v; return
        if a==0x3E: s.sp=(s.sp&0xFF)|(v<<8); return
        s.io[a]=v&0xFF; s.env.write(s,a,v)
    def rd(s,a):
        if a<0x20: return s.r[a]
        if a<0x60: return s.rd_io(a-0x20)
        if a==0x7A:
            v=s.mem[a]
            if s.adc_busy>s.cyc: v|=0x40
            else: v&=~0x40
            return v
        return s.mem[a]
    def wr(s,a,v):
        v&=0xFF
        if a<0x20: s.r[a]=v; return
        if a<0x60: s.wr_io(a-0x20,v); return
        if a==0x7A and v&0x40:
            s.adc_busy=s.cyc+13*128; ch=s.mem[0x7C]&0x0F
            s.mem[0x79]=s.env.adc(ch); v&=~0x40
        if a==0x61: s.env.clkpr.append(v)
        s.mem[a]=v
    def push(s,v): s.mem[s.sp]=v&0xFF; s.sp-=1
    def pop(s): s.sp+=1; return s.mem[s.sp]
    def skip(s):
        w=W[s.pc]; two=(w&0xFE0F) in (0x9200,0x9000) 
        s.pc+=2 if two else 1; s.cyc+=2 if two else 1
    def step(s):
        w=W[s.pc]; s.pc+=1; s.cyc+=1; r=s.r
        def sx(v,b): return v-(1<<b) if v&(1<<(b-1)) else v
        d5=(w>>4)&0x1F; r5=(w&0xF)|((w>>5)&0x10); d4=16+((w>>4)&0xF); K8=((w>>4)&0xF0)|(w&0xF)
        if w&0xF000==0xE000: r[d4]=K8
        elif w&0xF000==0x3000: t=r[d4]-K8; s.C=int(t<0); s.Z=int(t&0xFF==0)
        elif w&0xF000==0x6000: r[d4]|=K8; s.Z=int(r[d4]==0)
        elif w&0xF000==0x7000: r[d4]&=K8; s.Z=int(r[d4]==0)
        elif w&0xF800==0xB800: s.wr_io(((w>>5)&0x30)|(w&0xF),r[d5])
        elif w&0xF800==0xB000: r[d5]=s.rd_io(((w>>5)&0x30)|(w&0xF))
        elif w&0xFE0F==0x9200: s.wr(W[s.pc],r[d5]); s.pc+=1; s.cyc+=1
        elif w&0xFE0F==0x9000: r[d5]=s.rd(W[s.pc]); s.pc+=1; s.cyc+=1
        elif w&0xFC00==0x9800:
            A=(w>>3)&0x1F;b=w&7;op=(w>>8)&3; s.cyc+=1
            if op==0: s.wr_io(A,s.rd_io(A)&~(1<<b))
            elif op==2: s.wr_io(A,s.rd_io(A)|(1<<b))
            else:
                s.cyc-=1; bit=(s.rd_io(A)>>b)&1
                if (op==1 and bit==0) or (op==3 and bit==1): s.skip()
        elif w&0xF000==0xC000: s.pc+=sx(w&0xFFF,12); s.cyc+=1
        elif w&0xF000==0xD000: s.push(s.pc&0xFF); s.push(s.pc>>8); s.pc+=sx(w&0xFFF,12); s.cyc+=2
        elif w==0x9508: hi=s.pop(); lo=s.pop(); s.pc=(hi<<8)|lo; s.cyc+=3
        elif w&0xF800==0xF000 or w&0xF800==0xF400:
            k=sx((w>>3)&0x7F,7); bit=w&7; flag=s.Z if bit==1 else s.C
            take = (flag==1) if w&0x0400==0 else (flag==0)
            if take: s.pc+=k; s.cyc+=1
        elif w&0xFC00==0x1400: t=r[d5]-r[r5]; s.C=int(t<0); s.Z=int(t&0xFF==0)
        elif w&0xFC00==0x2C00: r[d5]=r[r5]
        elif w&0xFC00==0x2800: r[d5]|=r[r5]; s.Z=int(r[d5]==0)
        elif w&0xFC00==0x2400: r[d5]^=r[r5]; s.Z=int(r[d5]==0)
        elif w&0xFC00==0x2000: r[d5]&=r[r5]; s.Z=int(r[d5]==0)
        elif w&0xFC00==0x0C00: t=r[d5]+r[r5]; s.C=int(t>0xFF); r[d5]=t&0xFF; s.Z=int(r[d5]==0)
        elif w&0xFC00==0x1C00: t=r[d5]+r[r5]+s.C; s.C=int(t>0xFF); r[d5]=t&0xFF; s.Z=int(r[d5]==0)
        elif w&0xFE0F==0x9403: r[d5]=(r[d5]+1)&0xFF; s.Z=int(r[d5]==0)
        elif w&0xFE0F==0x940A: r[d5]=(r[d5]-1)&0xFF; s.Z=int(r[d5]==0)
        elif w&0xFF00==0x9700:
            d=24+2*((w>>4)&3);K=((w>>2)&0x30)|(w&0xF); v=r[d]|(r[d+1]<<8); t=v-K
            s.C=int(t<0); t&=0xFFFF; r[d]=t&0xFF; r[d+1]=t>>8; s.Z=int(t==0); s.cyc+=1
        elif w&0xFE0F==0x920D: x=r[26]|(r[27]<<8); s.wr(x,r[d5]); x+=1; r[26]=x&0xFF; r[27]=x>>8; s.cyc+=1
        elif w&0xFC08==0xFC00:
            bit=(r[d5]>>(w&7))&1
            if (w&0x0200 and bit) or (not w&0x0200 and not bit): s.skip()
        else: raise Exception('unknown %04X at %d'%(w,s.pc-1))
class Env:
    def __init__(s,adcs,manual=False,tank_empty=False,hum=60,temp=27,dht=True):
        s.adcs=adcs; s.manual=manual; s.tank=tank_empty; s.hum=hum; s.temp=temp; s.dht=dht
        s.clkpr=[]; s.portb_log=[]; s.release=None; s.wave=None
    def adc(s,ch): return s.adcs[ch]
    def write(s,cpu,a,v):
        if a==0x05: s.portb_log.append((cpu.cyc,v))
        if a==0x0A:  # DDRD
            if not (v&4) and s.dht: s.start_wave(cpu.cyc)
    def start_wave(s,t0):
        us=8  # cycles per us
        b=[s.hum,0,s.temp,0]; b.append(sum(b)&0xFF)
        seg=[(1,30),(0,80),(1,80)]
        for byte in b:
            for i in range(7,-1,-1):
                seg.append((0,50)); seg.append((1,70 if (byte>>i)&1 else 26))
        seg.append((0,50))
        s.wave=[]; t=t0
        for lvl,d in seg: s.wave.append((t,t+d*us,lvl)); t+=d*us
        s.wave_end=t
    def pind(s,cpu):
        v=0
        ddrd=cpu.io.get(0x0A,0)
        if ddrd&4: v|= cpu.io.get(0x0B,0)&4
        else:
            lvl=1
            if s.wave and cpu.cyc<s.wave_end:
                for a,b,l in s.wave:
                    if a<=cpu.cyc<b: lvl=l;break
            v|=4 if lvl else 0
        v|=0 if s.manual else 8
        v|=0 if s.tank else 0x10
        return v
def run(env,cycles=8_000_000*2):
    c=AVR(env)
    while c.cyc<cycles: c.step()
    return c
if __name__=='__main__':
    cases=[('all wet',Env([128,128,128,64])),
           ('zone1+3 dry',Env([200,128,180,64])),
           ('dry but humid 92%',Env([200,200,200,200],hum=92)),
           ('manual',Env([0,0,0,0],manual=True)),
           ('tank empty',Env([200,200,200,200],tank_empty=True)),
           ('dht absent, zone4 dry',Env([0,0,0,255],dht=False))]
    for name,e in cases:
        c=run(e,8_000_000)  # 1 s
        print('%-22s PORTB=%s hum=%d temp=%d clkpr=%s'%(name,bin(e.portb_log[-1][1]) if e.portb_log else None,c.mem[0x100],c.mem[0x102],e.clkpr))
