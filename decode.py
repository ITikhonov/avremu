
# bits(x,0/1/2/3)
def b4(x):
	return [0xf&(x>>(a*4)) for a in range(4)]

def sign(n,x):
	m=(1<<n)
	if x<(m/2): return x
	return -(m-x)

def decode_i4_a4_b4_a4(x):
	b=b4(x)
	return 16+b[1],(b[2]<<4)|b[0]

def decode_i7_5_i4_16(x,y):
	return y,0x1f&(x>>4)

def decode_i8_5_3(x):
	return (x>>3)&0b11111,x&0b111

def decode_i4_s12(x):
	return sign(12,x&0xfff),

def decode_i7_5_i4(x):
	return (x>>4)&0b11111,

def decode_i8_4_4(x):
	return (x>>4)&0b1111, x&0b1111


def decode_i6_s7_i3(x):
	return sign(7,(x>>3)&0b1111111),

def decode_i5_a2_b5_a4(x):
	return ((x>>5)&0b110000)|(x&0b1111),(x>>4)&0b11111

def decode_i4_a4_b4_a4(x):
	return ((x>>4)&0xf0)|(x&0xf),(x>>4)&0xf

def decode_i6_a1_b5_a4(x):
	return ((x>>5)&0x10)|(x&0xf),(x>>4)&0b11111

def decode_i8_a2_b2_a4(x):
	return ((x>>2)&0xb110000)|(x&0xf),(x>>4)&0b11

def d(p,x):
	a=0
	b=0
	for i in p.split():
		l=int(i[1:])
		if i[0]=='a':
			a=(a<<l)|((x&0xffff)>>(16-l))
		elif i[0]=='b':
			b=(b<<l)|((x&0xffff)>>(16-l))
		x<<=l
	if 'b' in p: return (a,b)
	return (a,)
			

def decode_i2_a1_i1_a2_i1_b5_i1_a3(x):
	return 
	

def load(x):
	f=open(x).read()
	prog=[(ord(f[i+1])<<8)|ord(f[i]) for i in range(0,len(f),2)]
	return prog

from os import system

def load_and_decode(name,ignorebefore):
	prog=load(name)
	asm=[]
	size=len(prog)
	while prog:
		offset=(size-len(prog))*2
		system('grep "%4x:" %s.dump'%(offset,name[:-4]))
		x=prog.pop(0)
		y=decode(x)
		if not y:
			try: y=decode32(x,prog.pop(0))
			except:
				if offset < ignorebefore:
					continue
				print
				raise Exception('Unknown %s at %04x'%(bin(x)[2:].zfill(16),offset))
		print y
		asm.append(y)
	return asm

def decode(x):
	if (x>>12)==0b1110: i=('LDI',decode_i4_a4_b4_a4(x))
	elif (x&0xff00)==0b1001101000000000: i=('SBI',decode_i8_5_3(x))
	elif (x&0xff00)==0b1001100000000000: i=('CBI',decode_i8_5_3(x))
	elif (x&0xf000)==0b1101000000000000: i=('RCALL',decode_i4_s12(x))
	elif (x&0xf000)==0b1100000000000000: i=('RJMP',decode_i4_s12(x))
	elif (x&0xfe0f)==0b1001010000001010: i=('DEC',decode_i7_5_i4(x))
	elif (x&0xff00)==0b0000000100000000: i=('MOVW',decode_i8_4_4(x))
	elif (x&0xf800)==0b1011100000000000: i=('OUT',decode_i5_a2_b5_a4(x))
	elif (x&0xfe0f)==0b1001000000000101: i=('LPMZP',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1001000000000100: i=('LPMZ',decode_i7_5_i4(x))


	elif (x&0xfe0f)==0b1001000000000001: i=('LDZP',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1001000000001100: i=('LDX',decode_i7_5_i4(x))
	elif (x&0b1101001000001000)==0b1000000000000000: i=('LDDZ',d('i2 a1 i1 a2 i1 b5 i1 a3',x))

	elif (x&0xfe0f)==0b1001001000001101: i=('STXP',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1001001000000001: i=('STZP',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1001001000001100: i=('STX',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1000001000001000: i=('STY',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1000001000000000: i=('STZ',decode_i7_5_i4(x))
	elif (x&0b1101001000001000)==0b1000001000001000: i=('STDY',d('i2 a1 i1 a2 i1 b5 i1 a3',x))
	elif (x&0b1101001000001000)==0b1000001000000000: i=('STDZ',d('i2 a1 i1 a2 i1 b5 i1 a3',x))

	elif (x&0xf000)==0b0011000000000000: i=('CPI',decode_i4_a4_b4_a4(x))
	elif (x&0xf000)==0b0111000000000000: i=('ANDI',decode_i4_a4_b4_a4(x))
	elif (x&0xf000)==0b0110000000000000: i=('ORI',decode_i4_a4_b4_a4(x))
	elif (x&0xf000)==0b0100000000000000: i=('SBCI',decode_i4_a4_b4_a4(x))
	elif (x&0xfc00)==0b0000010000000000: i=('CPC',decode_i6_a1_b5_a4(x))
	elif (x&0xfc00)==0b0001110000000000: i=('ADC',decode_i6_a1_b5_a4(x))
	elif (x&0xfe0f)==0b1001001000001111: i=('PUSH',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1001000000001111: i=('POP',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1001010000000110: i=('LSR',decode_i7_5_i4(x))
	elif (x&0xfe0f)==0b1001010000000111: i=('ROR',decode_i7_5_i4(x))
	elif (x&0xf800)==0b1011000000000000: i=('IN',decode_i5_a2_b5_a4(x))
	elif (x&0xfc00)==0b0010010000000000: i=('EOR',decode_i6_a1_b5_a4(x))
	elif (x&0xfc00)==0b0010000000000000: i=('AND',decode_i6_a1_b5_a4(x))
	elif (x&0xfc00)==0b0000110000000000: i=('ADD',decode_i6_a1_b5_a4(x))
	elif (x&0xfc00)==0b0001100000000000: i=('SUB',decode_i6_a1_b5_a4(x))
	elif (x&0xf000)==0b0101000000000000: i=('SUBI',decode_i4_a4_b4_a4(x))
	elif (x&0xf800)==0b1010100000000000: i=('STS16',decode_i5_a3_b4_a4(x))
	elif (x&0xff00)==0b1001011100000000: i=('SBIW',decode_i8_a2_b2_a4(x))
	elif (x&0xfc00)==0b0010110000000000: i=('MOV',decode_i6_a1_b5_a4(x))
	elif (x&0xfc00)==0b0010100000000000: i=('OR',decode_i6_a1_b5_a4(x))
	elif (x&0xfc00)==0b0001010000000000: i=('CP',decode_i6_a1_b5_a4(x))
	elif (x&0xff00)==0b1001011000000000: i=('ADIW',d('i8 a2 b2 a4',x))

	elif (x&0xfe08)==0b1111111000000000: i=('SBRS',d('i7 a5 i1 b3',x))
	elif (x&0xfe08)==0b1111110000000000: i=('SBRC',d('i7 a5 i1 b3',x))

	elif (x&0xfc07)==0b1111010000000001: i=('BRNE',decode_i6_s7_i3(x))
	elif (x&0xfc07)==0b1111000000000000: i=('BRCS',decode_i6_s7_i3(x))
	elif (x&0xfc07)==0b1111000000000001: i=('BREQ',decode_i6_s7_i3(x))
	elif (x&0xfc07)==0b1111010000000100: i=('BRGE',decode_i6_s7_i3(x))
	elif (x&0xfc07)==0b1111010000000000: i=('BRCC',decode_i6_s7_i3(x))
	elif (x&0xfc07)==0b1111010000000010: i=('BRPL',decode_i6_s7_i3(x))


	elif x==0b1001010100001000: i=('RET',())
	elif x==0b1001010100011000: i=('RETI',())
	elif x==0b1001010000001000: i=('SEC',())
	elif x==0b1001010001111000: i=('SEI',())
	elif x==0b1001010011111000: i=('CLI',())
	elif x==0: i=('NOP',())
	else: i=None
	return i

class UnkI(Exception):
	pass

def decode32(x,x1):
	if (x&0xfe0f)==0b1001001000000000: i=('STS',decode_i7_5_i4_16(x,x1))
	elif (x&0xfe0f)==0b1001000000000000: i=('LDS',decode_i7_5_i4_16(x,x1))
	else:
		raise UnkI()
	return i

if __name__=='__main__':
	from sys import argv
	asm=load_and_decode(argv[1],int(argv[2],16))
	for x in asm:
		print '%25s'%(str(x),)

"""
ADD 	Rd, Rr 	Add without Carry 	RdRd + Rr 	Z,C,N,V,H 	1
ADC 	Rd, Rr 	Add with Carry 	RdRd + Rr + C 	Z,C,N,V,H 	1
ADIW 	Rd, K 	Add Immediate to Word 	Rd+1:RdRd+1:Rd + K 	Z,C,N,V 	2
SUB 	Rd, Rr 	Subtract without Carry 	RdRd - Rr 	Z,C,N,V,H 	1
SUBI 	Rd, K 	Subtract Immediate 	RdRd - K 	Z,C,N,V,H 	1
SBC 	Rd, Rr 	Subtract with Carry 	RdRd - Rr - C 	Z,C,N,V,H 	1
SBCI 	Rd, K 	Subtract Immediate with Carry 	RdRd - K - C 	Z,C,N,V,H 	1
SBIW 	Rd, K 	Subtract Immediate from Word 	Rd+1:RdRd+1:Rd - K 	Z,C,N,V 	2
AND 	Rd, Rr 	Logical AND 	RdRd * Rr 	Z,N,V 	1
ANDI 	Rd, K 	Logical AND with Immediate 	RdRd * K 	Z,N,V 	1
OR 	Rd, Rr 	Logical OR 	RdRd v Rr 	Z,N,V 	1
ORI 	Rd, K 	Logical OR with Immediate 	Rd Rd v K 	Z,N,V 	1
EOR 	Rd, Rr 	Exclusive OR 	RdRdRr 	Z,N,V 	1
COM 	Rd 	One's Complement 	Rd$FF - Rd 	Z,C,N,V 	1
NEG 	Rd 	Two's Complement 	Rd$00 - Rd 	Z,C,N,V,H 	1
SBR 	Rd,K 	Set Bit(s) in Register 	RdRd v K 	Z,N,V 	1
CBR 	Rd,K 	Clear Bit(s) in Register 	RdRd * ($FFh - K) 	Z,N,V 	1
INC 	Rd 	Increment 	RdRd + 1 	Z,N,V 	1
DEC 	Rd 	Decrement 	RdRd - 1 	Z,N,V 	1
TST 	Rd 	Test for Zero or Minus 	RdRd * Rd 	Z,N,V 	1
CLR 	Rd 	Clear Register 	RdRdRd 	Z,N,V 	1
SER 	Rd 	Set Register 	Rd$FF 	None 	1
CP 	Rd,Rr 	Compare 	Rd - Rr 	Z,C,N,V,H 	1
CPC 	Rd,Rr 	Compare with Carry 	Rd - Rr - C 	Z,C,N,V,H 	1
CPI 	Rd,K 	Compare with Immediate 	Rd - K 	Z,C,N,V,H 	1
RJMP 	k 	Relative Jump 	PCPC + k + 1 	None 	2
IJMP 		Indirect Jump to (Z) 	PCZ 	None 	2
JMP 	k 	Jump 	PCk 	None 	3
RCALL 	k 	Relative Call Subroutine 	PCPC + k + 1 	None 	3
ICALL 		Indirect Call to (Z) 	PCZ 	None 	3
CALL 	k 	Call Subroutine 	PCk 	None 	4
RET 		Subroutine Return 	PCSTACK 	None 	4
RETI 		Interrupt Return 	PCSTACK 	I 	4
CPSE 	Rd,Rr 	Compare, Skip if Equal 	if (Rd = Rr) PCPC + 2 or 3 	None 	1 / 2 / 3
SBRC 	Rr, b 	Skip if Bit in Register Cleared 	if (Rr(b)=0) PCPC + 2 or 3 	None 	1 / 2 / 3
SBRS 	Rr, b 	Skip if Bit in Register Set 	if (Rr(b)=1) PCPC + 2 or 3 	None 	1 / 2 / 3
SBIC 	P, b 	Skip if Bit in I/O Register Cleared 	if(I/O(P,b)=0) PCPC + 2 or 3 	None 	1 / 2 / 3
SBIS 	P, b 	Skip if Bit in I/O Register Set 	If(I/O(P,b)=1) PCPC + 2 or 3 	None 	1 / 2 / 3
BRBS 	s, k 	Branch if Status Flag Set 	if (SREG(s) = 1) then PCPC+k + 1 	None 	1 / 2
BRBC 	s, k 	Branch if Status Flag Cleared 	if (SREG(s) = 0) then PCPC+k + 1 	None 	1 / 2
BREQ 	k 	Branch if Equal 	if (Z = 1) then PCPC + k + 1 	None 	1 / 2
BRNE 	k 	Branch if Not Equal 	if (Z = 0) then PCPC + k + 1 	None 	1 / 2
BRCS 	k 	Branch if Carry Set 	if (C = 1) then PCPC + k + 1 	None 	1 / 2
BRCC 	k 	Branch if Carry Cleared 	if (C = 0) then PCPC + k + 1 	None 	1 / 2
BRSH 	k 	Branch if Same or Higher 	if (C = 0) then PC PC + k + 1 	None 	1 / 2
BRLO 	k 	Branch if Lower 	if (C = 1) then PCPC + k + 1 	None 	1 / 2
BRMI 	k 	Branch if Minus 	if (N = 1) then PCPC + k + 1 	None 	1 / 2
BRPL 	k 	Branch if Plus 	if (N = 0) then PCPC + k + 1 	None 	1 / 2
BRGE 	k 	Branch if Greater or Equal, Signed 	if (NV= 0) then PCPC+ k + 1 	None 	1 / 2
BRLT 	k 	Branch if Less Than, Signed 	if (NV= 1) then PCPC + k + 1 	None 	1 / 2
BRHS 	k 	Branch if Half Carry Flag Set 	if (H = 1) then PCPC + k + 1 	None 	1 / 2
BRHC 	k 	Branch if Half Carry Flag Cleared 	if (H = 0) then PCPC + k + 1 	None 	1 / 2
BRTS 	k 	Branch if T Flag Set 	if (T = 1) then PCPC + k + 1 	None 	1 / 2
BRTC 	k 	Branch if T Flag Cleared 	if (T = 0) then PCPC + k + 1 	None 	1 / 2
BRVS 	k 	Branch if Overflow Flag is Set 	if (V = 1) then PCPC + k + 1 	None 	1 / 2
BRVC 	k 	Branch if Overflow Flag is Cleared 	if (V = 0) then PCPC + k + 1 	None 	1 / 2
BRIE 	k 	Branch if Interrupt Enabled 	if ( I = 1) then PCPC + k + 1 	None 	1 / 2
BRID 	k 	Branch if Interrupt Disabled 	if ( I = 0) then PCPC + k + 1 	None 	1 / 2
MOV 	Rd, Rr 	Copy Register 	RdRr 	None 	1
LDI 	Rd, K 	Load Immediate 	RdK 	None 	1
LDS 	Rd, k 	Load Direct from SRAM 	Rd(k) 	None 	3
LD 	Rd, X 	Load Indirect 	Rd(X) 	None 	2
LD 	Rd, X+ 	Load Indirect and Post-Increment 	Rd(X), XX + 1 	None 	2
LD 	Rd, -X 	Load Indirect and Pre-Decrement 	XX - 1, Rd (X) 	None 	2
LD 	Rd, Y 	Load Indirect 	Rd(Y) 	None 	2
LD 	Rd, Y+ 	Load Indirect and Post-Increment 	Rd(Y), YY + 1 	None 	2
LD 	Rd, -Y 	Load Indirect and Pre-Decrement 	YY - 1, Rd(Y) 	None 	2
LDD 	Rd,Y+q 	Load Indirect with Displacement 	Rd(Y + q) 	None 	2
LD 	Rd, Z 	Load Indirect 	Rd(Z) 	None 	2
LD 	Rd, Z+ 	Load Indirect and Post-Increment 	Rd(Z), ZZ+1 	None 	2
LD 	Rd, -Z 	Load Indirect and Pre-Decrement 	ZZ - 1, Rd(Z) 	None 	2
LDD 	Rd, Z+q 	Load Indirect with Displacement 	Rd(Z + q) 	None 	2
STS 	k, Rr 	Store Direct to SRAM 	Rd(k) 	None 	3
ST 	X, Rr 	Store Indirect 	(X)Rr 	None 	2
ST 	X+, Rr 	Store Indirect and Post-Increment 	(X)Rr, XX + 1 	None 	2
ST 	-X, Rr 	Store Indirect and Pre-Decrement 	XX - 1, (X) Rr 	None 	2
ST 	Y, Rr 	Store Indirect 	(Y)Rr 	None 	2
ST 	Y+, Rr 	Store Indirect and Post-Increment 	(Y)Rr, YY + 1 	None 	2
ST 	-Y, Rr 	Store Indirect and Pre-Decrement 	YY - 1, (Y)Rr 	None 	2
STD 	Y+q,Rr 	Store Indirect with Displacement 	(Y + q)Rr 	None 	2
ST 	Z, Rr 	Store Indirect 	(Z)Rr 	None 	2
ST 	Z+, Rr 	Store Indirect and Post-Increment 	(Z)Rr, ZZ + 1 	None 	2
ST 	-Z, Rr 	Store Indirect and Pre-Decrement 	ZZ - 1, (Z)Rr 	None 	2
STD 	Z+q,Rr 	Store Indirect with Displacement 	(Z + q)Rr 	None 	2
LPM 		Load Program Memory 	R0(Z) 	None 	3
IN 	Rd, P 	In Port 	RdP 	None 	1
OUT 	P, Rr 	Out Port 	PRr 	None 	1
PUSH 	Rr 	Push Register on Stack 	STACKRr 	None 	2
POP 	Rd 	Pop Register from Stack 	RdSTACK 	None 	2
LSL 	Rd 	Logical Shift Left 	Rd(n+1)Rd(n),Rd(0)0,CRd(7) 	Z,C,N,V,H 	1
LSR 	Rd 	Logical Shift Right 	Rd(n)Rd(n+1),Rd(7)0,CRd(0) 	Z,C,N,V 	1
ROL 	Rd 	Rotate Left Through Carry 	Rd(0)C,Rd(n+1)Rd(n),CRd(7) 	Z,C,N,V,H 	1
ROR 	Rd 	Rotate Right Through Carry 	Rd(7)C,Rd(n)Rd(n+1),CRd(0) 	Z,C,N,V 	1
ASR 	Rd 	Arithmetic Shift Right 	Rd(n)Rd(n+1), n=0..6 	Z,C,N,V 	1
SWAP 	Rd 	Swap Nibbles 	Rd(3..0)Rd(7..4) 	None 	1
BSET 	s 	Flag Set 	SREG(s)1 	SREG(s) 	1
BCLR 	s 	Flag Clear 	SREG(s)0 	SREG(s) 	1
SBI 	P, b 	Set Bit in I/O Register 	I/O(P, b)1 	None 	2
CBI 	P, b 	Clear Bit in I/O Register 	I/O(P, b)0 	None 	2
BST 	Rr, b 	Bit Store from Register to T 	TRr(b) 	T 	1
BLD 	Rd, b 	Bit load from T to Register 	Rd(b)T 	None 	1
SEC 		Set Carry 	C1 	C 	1
CLC 		Clear Carry 	C0 	C 	1
SEN 		Set Negative Flag 	N1 	N 	1
CLN 		Clear Negative Flag 	N0 	N 	1
SEZ 		Set Zero Flag 	Z1 	Z 	1
CLZ 		Clear Zero Flag 	Z0 	Z 	1
SEI 		Global Interrupt Enable 	I1 	I 	1
CLI 		Global Interrupt Disable 	I0 	I 	1
SES 		Set Signed Test Flag 	S1 	S 	1
CLS 		Clear Signed Test Flag 	S0 	S 	1
SEV 		Set Two's Complement Overflow 	V1 	V 	1
CLV 		Clear Two's Complement Overflow 	V0 	V 	1
SET 		Set T in SREG 	T1 	T 	1
CLT 		Clear T in SREG 	T0 	T 	1
SEH 		Set Half Carry Flag in SREG 	H1 	H 	1
CLH 		Clear Half Carry Flag in SREG 	H0 	H 	1
NOP 		No Operation 	None 	1
SLEEP 		Sleep 	(see specific descr. for Sleep) 	None 	1
WDR 		Watchdog 	Reset (see specific descr. for WDR) 	None 	1
"""
