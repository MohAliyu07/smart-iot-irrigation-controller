from asm import *
PINB,DDRB,PORTB,PIND,DDRD,PORTD,SPL,SPH=0x03,0x04,0x05,0x09,0x0A,0x0B,0x3D,0x3E
CLKPR,ADCL,ADCH,ADCSRA,ADMUX,DIDR0=0x61,0x78,0x79,0x7A,0x7C,0x7E
HUM,TMP,BUF=0x100,0x102,0x110
DRY_TH=102      # ADCH >= 102 (~40%) -> soil dry -> open valve
HUM_TH=90       # humidity >= 90% -> skip irrigation (rain)
BIT_TH=77       # high-time counter threshold (0.625us/count) -> ~48us
L('reset')
I('ldi','r16',0x08); I('out',SPH,'r16'); I('ldi','r16',0xFF); I('out',SPL,'r16')
I('ldi','r16',0x80); I('ldi','r17',0x00); I('sts',CLKPR,'r16'); I('sts',CLKPR,'r17')   # 8 MHz
I('ldi','r16',0x3F); I('out',DDRB,'r16'); I('clr','r16'); I('out',PORTB,'r16')
I('out',DDRD,'r16'); I('ldi','r16',0x1C); I('out',PORTD,'r16')                          # pull-ups PD2..PD4
I('ldi','r16',0x0F); I('sts',DIDR0,'r16')
I('ldi','r16',0x87); I('sts',ADCSRA,'r16')
I('clr','r16'); I('sts',HUM,'r16'); I('sts',TMP,'r16')
L('main')
I('rcall','dht_read')
I('clr','r22')
for ch,bit in ((0,0x02),(1,0x04),(2,0x08),(3,0x10)):
    I('ldi','r17',ch); I('rcall','adc_read'); I('cpi','r23',DRY_TH); I('brlo','wet%d'%ch); I('ori','r22',bit); L('wet%d'%ch)
I('lds','r16',HUM); I('cpi','r16',HUM_TH); I('brlo','nohum'); I('clr','r22'); L('nohum')
I('sbis',PIND,3); I('ldi','r22',0x1E)              # manual button (active low)
I('tst','r22'); I('breq','nopump'); I('ori','r22',0x01); L('nopump')
I('sbic',PIND,4); I('rjmp','tankok'); I('ldi','r22',0x20); L('tankok')   # tank empty -> alarm only
I('out',PORTB,'r22')
I('ldi','r24',0xF4); I('ldi','r25',0x01); I('rcall','delay_ms')      # 500 ms
I('rjmp','main')
# --- ADC: channel in r17, result (8-bit, left adjusted) in r23
L('adc_read')
I('mov','r16','r17'); I('ori','r16',0x60); I('sts',ADMUX,'r16')
I('lds','r16',ADCSRA); I('ori','r16',0x40); I('sts',ADCSRA,'r16')
L('adw'); I('lds','r16',ADCSRA); I('sbrc','r16',6); I('rjmp','adw')
I('lds','r23',ADCH); I('ret')
# --- delay r25:r24 ms @8MHz
L('delay_ms')
L('dl1'); I('ldi','r30',1999&0xFF); I('ldi','r31',1999>>8)
L('dl2'); I('sbiw','r30',1); I('brne','dl2')
I('sbiw','r24',1); I('brne','dl1'); I('ret')
# --- DHT11 on PD2
L('dht_read')
I('sbi',DDRD,2); I('cbi',PORTD,2)
I('ldi','r24',20); I('ldi','r25',0); I('rcall','delay_ms')
I('cbi',DDRD,2); I('sbi',PORTD,2)
def wait_while(level,name):
    # wait while pin == level; timeout -> fail
    I('clr','r18'); L(name)
    I('sbis' if level==1 else 'sbic',PIND,2)   # stay while pin==level, else jump out
    I('rjmp',name+'d')
    I('inc','r18'); I('brne',name); I('rjmp','dht_fail'); L(name+'d')
wait_while(1,'w1'); wait_while(0,'w2'); wait_while(1,'w3')
I('ldi','r26',BUF&0xFF); I('ldi','r27',BUF>>8); I('ldi','r21',5)
L('byteloop'); I('ldi','r20',8); I('clr','r19')
L('bitloop')
wait_while(0,'w4'); wait_while(1,'w5')
I('lsl','r19'); I('cpi','r18',BIT_TH); I('brlo','b0'); I('ori','r19',1); L('b0')
I('dec','r20'); I('brne','bitloop')
I('stxp','r19'); I('dec','r21'); I('brne','byteloop')
I('lds','r16',BUF); 
for k in (1,2,3): I('lds','r17',BUF+k); I('add','r16','r17')
I('lds','r17',BUF+4); I('cp','r16','r17'); I('brne','dht_fail')
I('lds','r16',BUF); I('sts',HUM,'r16'); I('lds','r16',BUF+2); I('sts',TMP,'r16')
L('dht_fail'); I('ret')
words,labels=assemble()
if __name__=='__main__':
    open('irrigation_avr.hex','w').write(ihex(words))
    print(len(words),'words'); print(labels)
