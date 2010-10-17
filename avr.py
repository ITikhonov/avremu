
from sys import argv
from decode import load,decode,decode32

class AVR:
	MEM=[0]*2303
	FLASH=load(argv[1])
	PC=0
	CLOCKS=0

	def store(_):
		_.oMEM=_.MEM[:]

	def compare(_):
		for x in range(0x20,len(_.MEM)):
			if x==0x3f: continue
			if x==0x3d: continue
			if x==0x3e: continue
			if _.MEM[x]!=_.oMEM[x]:
				print '    [%04x] %02x -> %02x'%(x,_.oMEM[x],_.MEM[x])
A=AVR()

###################################################33

def getSP():
	return (A.MEM[0x3e]<<8)+A.MEM[0x3d]

def setSP(x):
	A.MEM[0x3e]=x>>8
	A.MEM[0x3d]=x&0xff

SREG=0x3F
FZ=2

###################################################33

def avr_LDI(reg,val):
	A.MEM[reg]=val
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

	A.store()
	cl=locals()['avr_'+y[0]](*y[1])
	A.compare()
	A.CLOCKS+=cl

	now=time()
	d=now-start
	if d > 1:
		print A.CLOCKS/d,'clocks per second'
		A.CLOCKS=0
		start=now

