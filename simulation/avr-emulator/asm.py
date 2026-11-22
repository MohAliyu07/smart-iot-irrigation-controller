# Minimal AVR assembler (subset) for ATmega328P irrigation firmware
import sys
PROG=[]  # (label or None, mnemonic, args)
def L(name): PROG.append(('label',name))
def I(m,*a): PROG.append(('ins',m,a))
TWO_WORD={'sts','lds','call','jmp'}
def size(m): return 2 if m in TWO_WORD else 1
def r(x): return int(x[1:]) if isinstance(x,str) else x
def enc(m,a,pc,labels):
    def rel(lbl,bits):
        k=labels[lbl]-(pc+1)
        lim=1<<(bits-1)
        assert -lim<=k<lim,(m,lbl,k)
        return k&((1<<bits)-1)
    if m=='ldi': d=r(a[0]);K=a[1]&0xFF;assert 16<=d<=31; return [0xE000|((K&0xF0)<<4)|((d-16)<<4)|(K&0xF)]
    if m=='cpi': d=r(a[0]);K=a[1]&0xFF;assert d>=16; return [0x3000|((K&0xF0)<<4)|((d-16)<<4)|(K&0xF)]
    if m=='ori': d=r(a[0]);K=a[1]&0xFF;assert d>=16; return [0x6000|((K&0xF0)<<4)|((d-16)<<4)|(K&0xF)]
    if m=='andi': d=r(a[0]);K=a[1]&0xFF;assert d>=16; return [0x7000|((K&0xF0)<<4)|((d-16)<<4)|(K&0xF)]
    if m=='out': A=a[0];rr=r(a[1]); return [0xB800|((A&0x30)<<5)|(rr<<4)|(A&0xF)]
    if m=='in': d=r(a[0]);A=a[1]; return [0xB000|((A&0x30)<<5)|(d<<4)|(A&0xF)]
    if m=='sts': k=a[0];rr=r(a[1]); return [0x9200|(rr<<4),k]
    if m=='lds': d=r(a[0]);k=a[1]; return [0x9000|(d<<4),k]
    if m in('sbi','cbi','sbis','sbic'):
        base={'cbi':0x9800,'sbic':0x9900,'sbi':0x9A00,'sbis':0x9B00}[m]; A,b=a; assert A<32; return [base|(A<<3)|b]
    if m=='rjmp': return [0xC000|rel(a[0],12)]
    if m=='rcall': return [0xD000|rel(a[0],12)]
    if m=='ret': return [0x9508]
    if m in('brne','breq','brsh','brlo'):
        base={'breq':0xF001,'brne':0xF401,'brlo':0xF000,'brsh':0xF400}[m]; return [base|(rel(a[0],7)<<3)]
    two={'cp':0x1400,'mov':0x2C00,'or':0x2800,'eor':0x2400,'and':0x2000,'add':0x0C00,'adc':0x1C00}
    if m in two: d=r(a[0]);rr=r(a[1]); return [two[m]|((rr&0x10)<<5)|(d<<4)|(rr&0xF)]
    if m=='clr': return enc('eor',(a[0],a[0]),pc,labels)
    if m=='lsl': return enc('add',(a[0],a[0]),pc,labels)
    if m=='rol': return enc('adc',(a[0],a[0]),pc,labels)
    if m=='tst': return enc('and',(a[0],a[0]),pc,labels)
    if m=='inc': return [0x9403|(r(a[0])<<4)]
    if m=='dec': return [0x940A|(r(a[0])<<4)]
    if m=='sbiw': d=r(a[0]);K=a[1]; return [0x9700|((K&0x30)<<2)|(((d-24)//2)<<4)|(K&0xF)]
    if m=='stxp': return [0x920D|(r(a[0])<<4)]   # st X+,Rr
    if m=='ldxp': return [0x900D|(r(a[0])<<4)]   # ld Rd,X+
    if m in('sbrs','sbrc'): base={'sbrc':0xFC00,'sbrs':0xFE00}[m]; return [base|(r(a[0])<<4)|a[1]]
    if m=='push': return [0x920F|(r(a[0])<<4)]
    if m=='pop': return [0x900F|(r(a[0])<<4)]
    raise ValueError(m)
def assemble():
    labels={};pc=0
    for it in PROG:
        if it[0]=='label': labels[it[1]]=pc
        else: pc+=size(it[1])
    words=[];pc=0
    for it in PROG:
        if it[0]=='ins':
            w=enc(it[1],it[2],pc,labels); assert len(w)==size(it[1]); words+=w; pc+=len(w)
    return words,labels
def ihex(words):
    data=b''.join(w.to_bytes(2,'little') for w in words)
    out=[]
    for off in range(0,len(data),16):
        chunk=data[off:off+16]
        rec=[len(chunk),(off>>8)&0xFF,off&0xFF,0]+list(chunk)
        out.append(':'+''.join('%02X'%b for b in rec)+'%02X'%((-sum(rec))&0xFF))
    out.append(':00000001FF')
    return '\n'.join(out)+'\n'
