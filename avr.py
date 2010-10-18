
from sys import argv
from decode import load,decode,decode32

class UnimpIOE(Exception):
	pass

class IO:
	def __init__(_,a): _.addr=a

class UnimpIO(IO):
	def read(_): raise UnimpIOE('IO read unimplemented for %02x'%(_.addr,))
	def write(_,y): raise UnimpIOE('IO write unimplemented for %02x (%02x)'%(_.addr,y))

class IO_5e(IO):
	"SPH"
	def read(_): return _.sp
	def write(_,y): _.sp=y
	def log(_,y): pass

class IO_5d(IO):
	"SPL"
	def read(_): return _.sp
	def write(_,y): _.sp=y
	def log(_,y): pass

class IO_61(IO):
	CLKPR=0
	def read(_): return _.CLKPR
	def write(_,y):
		if y==0x80: print '  CLKPCE'
		else: print '  CLKPS:%x'%(1<<(y&0xf))
		_.CLKPR=y

class IO_2a(IO):
	DDRD=0
	def read(_): return _.DDRD
	def write(_,y):
		for x in range(0,8):
			m=1<<x
			if y&m!=_.DDRD&m:
				if y&m: d='OUT'
				else: d='IN'
				print '  DDD%u:%s'%(x,d)
		_.DDRD=y

class IO_2b(IO):
	PORTD=0
	def read(_): return _.PORTD
	def write(_,y):
		DDRD=A.MEM[0x2a]
		for x in range(0,8):
			m=1<<x
			if y&m!=_.PORTD&m:
				assert DDRD&m
				if y&m: d='HIGH'
				else: d='LOW'
				print ' PORTD%u:%s'%(x,d)
		_.PORTD=y

class IO_5f(IO):
	SREG=0
	def read(_): return _.SREG
	def write(_,y):
		names='CZNVSHTI'
		changes=[]
		for x in range(0,8):
			m=1<<x
			if y&m: changes.append(names[x])
			else: changes.append('_')
		if changes:
			print '  SREG:'+(''.join(changes))
		_.SREG=y

class IO_d7(IO):
	UHWCON=0
	def read(_): return _.UHWCON
	def write(_,y):
		if y&1!=_.UHWCON&1:
			print '  USB Pad Regulator:',['DISABLED','ENABLED'][y&1]
		_.UHWCON=y

def rb(n,y,x,name,on='ENABLED',off='DISABLED'):
	m=1<<n
	if y&m!=x&m:
		if y&m: s=on
		else: s=off
		print '  %s: %s'%(name,s)

class IO_d8(IO):
	USBCON=0b00100000
	def read(_): return _.USBCON
	def write(_,y):
		rb(7,y,_.USBCON,'USB')
		rb(5,y,_.USBCON,'USB CLOCK','FREEZED','ENABLED')
		rb(4,y,_.USBCON,'USB VBUS PAD')
		rb(0,y,_.USBCON,'USB VBUS INT')
		_.USBCON=y
				
class IO_49(IO):
	PLLCSR=0
	def read(_): return _.PLLCSR
	def write(_,y):
		rb(4,y,_.PLLCSR,'PLL Prescaler','1:1','1:2')
		rb(1,y,_.PLLCSR,'PLL')
		_.PLLCSR=y
		

def getIO(x):
	f=globals().get('IO_%02x'%x)
	if f: return f(x)
	return UnimpIO(x)

AVRIO=[getIO(x) for x in range(0x20,0x100)]

print AVRIO[0x5e-0x20]

class AVRMEM:
	def __init__(_):
		_.REG=[0]*0x20
		_.MEM=[0]*0xa00

	def __getitem__(_,x):
		if x<0x20: return _.REG[x]
		if x>0xff: return _.MEM[x-0x100]
		return AVRIO[x-0x20].read()

	def __setitem__(_,x,y):
		if x<0x20:
			print "  r%u=%02x"%(x,y)
			_.REG[x]=y
		elif x>0xff:
			print "  [%04x]=%02x"%(x,y)
			_.MEM[x-0x100]=y
		else:
			if getattr(AVRIO[x-0x20],'log',None):
				AVRIO[x-0x20].log(y)
			else:
				print "  IO%02x=%02x"%(x,y)
			AVRIO[x-0x20].write(y)
		

class AVR:
	def __init__(_):
		_.MEM=AVRMEM()
		_.FLASH=load(argv[1])
		_.PC=0
		_.CLOCKS=0

	def store(_):
		_.oMEM=_.MEM.MEM[:]

	def compare(_):
		for x in range(0x0,len(_.MEM.MEM)):
			if _.MEM.MEM[x]!=_.oMEM[x]:
				print '    [%04x] %02x -> %02x'%(x,_.oMEM[x],_.MEM[x])
A=AVR()

###################################################33

def getSP():
	return (A.MEM[0x5e]<<8)+A.MEM[0x5d]

def setSP(x):
	A.MEM[0x5e]=x>>8
	A.MEM[0x5d]=x&0xff

class SREG:
	def __init__(_):
		s=A.MEM[0x5f]
		_.H=bool(s&0b00100000)
		_.S=bool(s&0b00010000)
		_.V=bool(s&0b00001000)
		_.N=bool(s&0b00000100)
		_.Z=bool(s&0b00000010)
		_.C=bool(s&0b00000001)

def setFLAGS(H=None,V=None,N=None,Z=None,C=None):
	s=A.MEM[0x5f]
	if H is not None: s=(s&0b11011111)|(bool(H)<<5)
	if V is not None: s=(s&0b11110111)|(bool(V)<<3)
	if N is not None: s=(s&0b11111011)|(bool(N)<<2)
	if Z is not None: s=(s&0b11111101)|(bool(Z)<<1)
	if C is not None: s=(s&0b11111110)|(bool(C)<<0)

	s=(s&0b11101111)|((bool(N)^bool(V))<<4) # S
	A.MEM[0x5f]=s

FZ=2

###################################################33

def avr_LDI(val,reg):
	A.MEM[0x10+reg]=val
	return 1

def avr_STS(addr,reg):
	A.MEM[addr]=A.MEM[reg]
	return 2

def avr_SBI(io,bit):
	A.MEM[0x20+io]|=1<<bit
	return 2

def avr_CBI(io,bit):
	A.MEM[0x20+io]&=~(1<<bit)
	return 2

def avr_RCALL(o):
	sp=getSP()
	A.MEM[sp]=A.PC&0xff
	A.MEM[sp-1]=(A.PC>>8)
	setSP(sp-2)
	A.PC+=o
	return 3

def avr_RET():
	sp=getSP()
	A.PC=(A.MEM[sp+1]<<8)|A.MEM[sp+2]
	setSP(sp+2)
	return 4

def avr_NOP():
	return 1

def avr_DEC(reg):
	v=A.MEM[reg]-1
	A.MEM[reg]=v
	
	setFLAGS(Z=(v==0),N=(v&0x80),V=(v==0x7f))
	return 1

def avr_BRNE(o):
	if not SREG().Z:
		A.PC+=o
		return 2
	return 1

def avr_RJMP(o):
	A.PC+=o
	return 2

def avr_EOR(rr,rd):
	v=(A.MEM[rr]^A.MEM[rd])&0xff
	A.MEM[rd]=v
	setFLAGS(Z=(v==0),N=(v&0x80),V=0)
	return 1

def avr_OUT(a,rr):
	A.MEM[0x20+a]=A.MEM[rr]
	return 1

def avr_CPI(k,rd):
	"""TODO: HalfCarry"""
	r=A.MEM[rd+0x10]
	v=(r-k)&0xff

	Rd7=r>>6
	K7=k>>6
	R7=v>>6

	setFLAGS(	C=((not Rd7)&K7)or(K7&R7)or(R7&(not Rd7)),
			N=R7,
			Z=(v==0),
			V=(Rd7&(not K7)&(not R7))or((not Rd7)&K7&R7))
	return 1

def avr_CPC(rr,rd):
	"""TODO: HalfCarry"""
	r=A.MEM[rr]
	d=A.MEM[rd]
	v=(d-r-SREG().C)&0xff

	Rd7=d>>6
	Rr7=r>>6
	R7=v>>6

	setFLAGS(	C=((not Rd7)&Rr7)or(Rr7&R7)or(R7&(not Rd7)),
			N=R7,
			Z=(v==0)&SREG().Z,
			V=(Rd7&(not Rr7)&(not R7))or((not Rd7)&Rr7&R7))
	return 1

def avr_LPMZP(rd):
	Z=(A.MEM[0x1f]<<8)|A.MEM[0x1e]
	u16=A.FLASH[Z/2]
	if Z&1: A.MEM[rd]=(u16>>8)&0xff
	else: A.MEM[rd]=u16&0xff
	Z+=1
	A.MEM[0x1e]=Z&0xff
	A.MEM[0x1f]=(Z>>8)&0xff
	return 3

def avr_STXP(rd):
	X=(A.MEM[0x1b]<<8)|A.MEM[0x1a]
	A.MEM[X]=rd
	X+=1
	A.MEM[0x1a]=X&0xff
	A.MEM[0x1b]=(X>>8)&0xff
	return 2

def avr_PUSH(rr):
	sp=getSP()
	A.MEM[sp]=A.MEM[rr]
	setSP(sp-1)
	return 2
	
	
###################################################33
	
###################################################33

def reset():
	setSP(0x8fe)

verbose=True

from time import time

reset()

start=time()
while True:
	x=A.FLASH[A.PC]
	if verbose: print '%04x:'%(A.PC*2,),
	y=decode(x); A.PC+=1
	if not y:
		y=decode32(x,A.FLASH[A.PC])
		A.PC+=1

	if verbose: print y

	#A.store()
	cl=locals()['avr_'+y[0]](*y[1])
	#A.compare()
	A.CLOCKS+=cl

	now=time()
	d=now-start
	if d > 1:
		print A.CLOCKS/d,'clocks per second'
		A.CLOCKS=0
		start=now

