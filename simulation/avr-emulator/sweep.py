import emu, json
from multiprocessing import Pool
F=8_000_000
def last(adcs,hum=60):
    e=emu.Env(adcs,hum=hum); c=emu.AVR(e)
    while c.cyc<0.6*F: c.step()
    return e.portb_log[-1][1]
def m(p): v=round(p*255/100); return (p,v,last([v,0,0,0]))
def h(hu): return (hu,last([200,200,200,200],hum=hu))
if __name__=='__main__':
    with Pool(8) as P:
        ms=P.map(m,[x/2 for x in range(60,101)])
        hs=P.map(h,list(range(80,100)))
    json.dump({'moist':ms,'hum':hs},open('sweep.json','w')); print(ms); print(hs)
