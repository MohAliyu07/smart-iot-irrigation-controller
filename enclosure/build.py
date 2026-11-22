import json, math, struct, numpy as np
MIL=0.0254; CX=CY=4771.5
P=[((x-CX)*MIL,(y-CY)*MIL) for x,y in json.load(open('/tmp/q.json'))]
def area(p): return 0.5*sum(p[i][0]*p[(i+1)%len(p)][1]-p[(i+1)%len(p)][0]*p[i][1] for i in range(len(p)))
if area(P)<0: P=P[::-1]
def resample(p,step):
    out=[];n=len(p)
    for i in range(n):
        a=np.array(p[i]);b=np.array(p[(i+1)%n]);L=np.linalg.norm(b-a);k=max(1,int(math.ceil(L/step)))
        for j in range(k): out.append(tuple(a+(b-a)*j/k))
    return out
P=resample(P,0.6)
def offset(p,d):
    n=len(p);q=[]
    for i in range(n):
        a=np.array(p[i-1]);b=np.array(p[i]);c=np.array(p[(i+1)%n])
        n1=np.array([(b-a)[1],-(b-a)[0]]);n1/=np.linalg.norm(n1)
        n2=np.array([(c-b)[1],-(c-b)[0]]);n2/=np.linalg.norm(n2)
        m=n1+n2;m/=np.linalg.norm(m);cos=max(0.3,np.dot(m,n1))
        q.append(tuple(b+m*d/cos))
    return q
tris=[]
def T(a,b,c): tris.append((a,b,c))
def Q(a,b,c,d): T(a,b,c);T(a,c,d)
def v3(p,z): return (p[0],p[1],z)

def ring(Pin,Pout,z0,z1,cuts=(),bottom=True,top=True):
    """wall solid between polygons (same count, CCW). cuts: list of (ang0,ang1,za,zb) deg"""
    n=len(Pout); zs=sorted(set([z0,z1]+[c for cut in cuts for c in cut[2:] if z0<c<z1]))
    ang=[math.degrees(math.atan2((Pout[i][1]+Pout[(i+1)%n][1])/2,(Pout[i][0]+Pout[(i+1)%n][0])/2)) for i in range(n)]
    def inang(a,a0,a1):
        return (a0<=a<=a1) or (a0<=a+360<=a1) or (a0<=a-360<=a1)
    def solid(i,j):
        if j<0 or j>=len(zs)-1: return False
        i%=n; zm=(zs[j]+zs[j+1])/2
        for a0,a1,za,zb in cuts:
            if inang(ang[i],a0,a1) and za<zm<zb: return False
        return True
    for i in range(n):
        i1=(i+1)%n
        for j in range(len(zs)-1):
            if not solid(i,j): continue
            za,zb=zs[j],zs[j+1]
            Ob0,Ob1,Ot1,Ot0=v3(Pout[i],za),v3(Pout[i1],za),v3(Pout[i1],zb),v3(Pout[i],zb)
            Ib0,Ib1,It1,It0=v3(Pin[i],za),v3(Pin[i1],za),v3(Pin[i1],zb),v3(Pin[i],zb)
            Q(Ob0,Ob1,Ot1,Ot0); Q(Ib1,Ib0,It0,It1)
            if not solid(i,j-1) and (j>0 or bottom): Q(Ob0,Ib0,Ib1,Ob1)
            if not solid(i,j+1) and (j<len(zs)-2 or top): Q(Ob0,Ob1,Ib1,Ib0) if False else Q(Ot0,Ot1,It1,It0)
            if not solid(i-1,j): Q(Ob0,Ot0,It0,Ib0)
            if not solid(i+1,j): Q(Ob1,Ib1,It1,Ot1)

# ---- polygon with holes triangulation (ear clipping with bridges)
def cross(o,a,b): return (a[0]-o[0])*(b[1]-o[1])-(a[1]-o[1])*(b[0]-o[0])
def pt_in_tri(p,a,b,c):
    d1,d2,d3=cross(a,b,p),cross(b,c,p),cross(c,a,p)
    return not ((d1<0 or d2<0 or d3<0) and (d1>0 or d2>0 or d3>0))
def seg_x(p1,p2,p3,p4):
    d1,d2,d3,d4=cross(p3,p4,p1),cross(p3,p4,p2),cross(p1,p2,p3),cross(p1,p2,p4)
    return (d1*d2<0) and (d3*d4<0)
def bridge(outer,hole):
    hi=max(range(len(hole)),key=lambda k:hole[k][0]); h=hole[hi]
    best=None
    for k,o in sorted(enumerate(outer),key=lambda t:(t[1][0]-h[0])**2+(t[1][1]-h[1])**2):
        ok=True
        for poly in (outer,hole):
            m=len(poly)
            for e in range(m):
                a,b=poly[e],poly[(e+1)%m]
                if a in (o,h) or b in (o,h): continue
                if seg_x(h,o,a,b): ok=False;break
            if not ok:break
        if ok: best=k;break
    k=best
    return outer[:k+1]+hole[hi:]+hole[:hi+1]+outer[k:]
def earclip(poly):
    idx=list(range(len(poly)));out=[];guard=0
    while len(idx)>3 and guard<100000:
        guard+=1;m=len(idx);found=False
        for t in range(m):
            i0,i1,i2=idx[t-1],idx[t],idx[(t+1)%m]
            a,b,c=poly[i0],poly[i1],poly[i2]
            cr=cross(a,b,c)
            if cr<-1e-9: continue
            if cr>1e-9 and any(pt_in_tri(poly[q],a,b,c) for q in idx if q not in(i0,i1,i2) and poly[q] not in (a,b,c)): continue
            out.append((a,b,c));del idx[t];found=True;break
        if not found: break
    if len(idx)==3: out.append(tuple(poly[q] for q in idx))
    return out
def plate(outer,holes,z0,z1):
    poly=list(outer)
    for h in holes:
        if area(h)>0: h=h[::-1]
        poly=bridge(poly,h)
    tt=earclip(poly)
    for a,b,c in tt: T(v3(a,z1),v3(b,z1),v3(c,z1)); T(v3(a,z0),v3(c,z0),v3(b,z0))
    for loop in [outer]+[h if area(h)<0 else h[::-1] for h in holes]:
        m=len(loop)
        for i in range(m):
            a,b=loop[i],loop[(i+1)%m]; Q(v3(a,z0),v3(b,z0),v3(b,z1),v3(a,z1))
    return len(tt)
def box(x0,y0,z0,x1,y1,z1):
    plate([(x0,y0),(x1,y0),(x1,y1),(x0,y1)],[],z0,z1)
def save(name):
    global tris
    with open(name,'wb') as f:
        f.write(b'\0'*80);f.write(struct.pack('<I',len(tris)))
        for a,b,c in tris:
            n=np.cross(np.subtract(b,a),np.subtract(c,a));L=np.linalg.norm(n);n=n/L if L else n
            f.write(struct.pack('<12fH',*n,*a,*b,*c,0))
    out=tris;tris=[];return out
def mm(x,y): return ((x-CX)*MIL,(y-CY)*MIL)
def angof(x,y): return math.degrees(math.atan2((y-CY),(x-CX)))
def span(x,y,w):  # angular cut width w mm at radius
    a=angof(x,y); r=47.0; d=math.degrees(w/2/r); return (a-d,a+d)

# ---------------- dims (mm)
WALL=2.0; GAP=1.0; FLOOR=2.0; H=23.0; BOARD_Z=7.0; BT=1.6
Pi=offset(P,GAP); Po=offset(P,GAP+WALL)
top=BOARD_Z+BT
cuts=[]
for (x,y) in [(3745,3787),(4210,3454),(4681,3341),(5136,3357),(5597,3606),(5782,4054)]:  # screw terminals
    a0,a1=span(x,y,12.5); cuts.append((a0,a1,top+0.4,top+8.5))
a0,a1=span(3291,4801,10.5); cuts.append((a0,a1,top+0.3,top+10.5))        # DC jack
a0,a1=span(3505,5517,10.0); cuts.append((a0,a1,top-0.2,top+3.8))         # USB-C
a0,a1=span(6250,4900,30.0); cuts.append((a0,a1,top+3.0,top+8.0))         # sensor cable slot
# BASE
ring(Pi,Po,FLOOR,H,cuts,bottom=False,top=True)
plate(Po,[],0,FLOOR)
# fix: floor top only under inner polygon -> rebuild floor manually
tris=[t for t in tris if not (all(abs(v[2]-FLOOR)<1e-9 for v in t))]  # drop floor top face
tt=earclip(list(Pi))
for a,b,c in tt: T(v3(a,FLOOR),v3(b,FLOOR),v3(c,FLOOR))
# floor annulus top between Po and Pi is covered by wall; floor side ok
ledge_in=offset(P,-0.6)
ring(ledge_in,Pi,FLOOR,BOARD_Z)
# mounting bosses for lid screws? -> corner bosses inside wall at 4 angles (M2.5 self-tap holes)
base=save('/home/claude/enc/enclosure_base.stl')
# LID (printed upside down: plate z0..2, rim 2..5)
sw=[(5656.3,5781.9),(5327.3,5785.6)]
holes=[]
for x,y in sw:
    cx,cy=mm(x,y); s=3.9; holes.append([(cx-s,cy-s),(cx+s,cy-s),(cx+s,cy+s),(cx-s,cy+s)])
# LED window strip near status LEDs LED1/LED2 (light pipes): small round-ish octagon holes
for x,y in [(4124.9,5428.5),(4209.9,5974.9)]:
    cx,cy=mm(x,y); r=1.6; holes.append([(cx+r*math.cos(k*math.pi/4),cy+r*math.sin(k*math.pi/4)) for k in range(8)])
plate(Po,holes,0,2.0)
rim_out=offset(P,GAP-0.25); rim_in=offset(P,GAP-1.65)
ring(rim_in,rim_out,2.0,5.0)
lid=save('/home/claude/enc/enclosure_lid.stl')
# plungers (x2): stem 7x7, flange 10x10
pl=[]
for k in range(2):
    ox=k*14
    box(ox-5,-5,0,ox+5,5,1.2); box(ox-3.4,-3.4,1.2,ox+3.4,3.4,16.0)
plung=save('/home/claude/enc/button_plungers.stl')
json.dump({'P':P,'Pi':Pi,'Po':Po,'cuts':cuts},open('/home/claude/enc/geo.json','w'))
print(len(base),len(lid),len(plung))
