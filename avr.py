
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

class IO_5d(IO):
	"SPL"
	def read(_): return _.sp
	def write(_,y): _.sp=y

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

class IO_3f(IO):
	SREG=0
	def read(_): return _.SREG
	def write(_,y): _.SREG=y

def getIO(x):
	f=globals().get('IO_%02x'%x)
	if f: return f(x)
	return UnimpIO(x)

AVRIO=[getIO(x) for x in range(0x20,0x100)]

print AVRIO[0x5e-0x20]

class AVRMEM:
	def __init__(_):
		_.REG=[0]*0x20
		_.MEM=[0]*2048

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
			#print "  IO%02x=%02x"%(x,y)
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

SREG=0x3F
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
	A.MEM[reg]-=1
	if A.MEM[reg]==0: A.MEM[SREG]|=FZ
	else: A.MEM[SREG]&=~FZ
	return 1

def avr_BRNE(o):
	if A.MEM[SREG]&FZ==0:
		A.PC+=o
		return 2
	return 1

def avr_RJMP(o):
	A.PC+=o
	return 2
	
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

