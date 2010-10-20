#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

#include <stdio.h>

#undef NDEBUG
#include <assert.h>

uint8_t mem[0xb00];
uint16_t flash[0x4000];
uint16_t pc;
uint64_t clocks;
int skipnext;

typedef uint8_t (*io_read_func)(uint8_t a);
typedef void (*io_write_func)(uint8_t a,uint8_t x);

void load(char *n) {
	int f=open(n,O_RDONLY);
	if(read(f,&flash,sizeof(flash))) ;
}

#define VERBOSE

#ifdef VERBOSE
void LOG(char *fmt,...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
#else 
#define LOG(...) ;
#endif

void rb(int bit,uint8_t n,uint8_t o,char *name,char *on, char *off) {
	int m=1<<bit;
	if((n&m)!=(o&m)) {
		printf("  %s: %s\n",name,(n&m)?on:off);
	}
}


/**************************************************************/

uint64_t plllock_at=0;

void schedules() {
	if(plllock_at && plllock_at<clocks) {
		plllock_at=0;
		mem[0x49]|=1;
	}
}
/**************************************************************/

#include "ioundef.inc"

#undef avr_IOW2a
void avr_IOW2a(uint8_t a,uint8_t x) {
	uint8_t o=mem[a];
	int m=1,n=0;
	for(;n<8;m<<=1,n++) {
		if((x&m)!=(o&m)) { printf("  DDRD%u: %s\n",n,(x&m)?"OUT":"IN"); }
	}
	mem[a]=x;
}

#undef avr_IOW2b
void avr_IOW2b(uint8_t a,uint8_t x) {
	uint8_t o=mem[a];
	uint8_t DDRD=mem[0x2a];
	int m=1,n=0;
	for(;n<8;m<<=1,n++) {
		if((x&m)!=(o&m)) {
			assert(DDRD&m);
			printf("  PORTD%u: %s\n",n,(x&m)?"HIGH":"LOW");
		}
	}
	mem[a]=x;
}

#undef avr_IOW5f
void avr_IOW5f(uint8_t a,uint8_t x) { mem[a]=x; }

#undef avr_IOW5e
void avr_IOW5e(uint8_t a,uint8_t x) { mem[a]=x; }

#undef avr_IOW5d
void avr_IOW5d(uint8_t a,uint8_t x) { mem[a]=x; }

#undef avr_IOW61
void avr_IOW61(uint8_t a,uint8_t x) {
	if(x==0x80) { printf("  CLKPCE\n"); }
	else { printf("  CLKPS:%x\n",1<<(x&0xf)); }
	mem[a]=x;
}

#undef avr_IOWd7
void avr_IOWd7(uint8_t a,uint8_t x) {
	if((x&1)!=(mem[a]&1)) { printf("  USB Pad Regulator: %s\n",x&1?"ENABLED":"DISABLED"); }
	mem[a]=x;
}

#undef avr_IOWd8
void avr_IOWd8(uint8_t a,uint8_t x) {
	uint8_t o=mem[a];
	rb(7,x,o,"USB","ON","OFF");
	rb(5,x,o,"USB CLOCK","FREEZED","ENABLED");
	rb(4,x,o,"USB VBUS PAD","ON","OFF");
	rb(0,x,o,"USB VBUS INT","ON","OFF");
	mem[a]=x;
}

#undef avr_IOR49
uint8_t avr_IOR49(uint8_t a) {
	return mem[a];
}

#undef avr_IOW49
void avr_IOW49(uint8_t a,uint8_t x) {
	uint8_t o=mem[a];
	x=(x&0xfe)|(o&1);

	rb(4,x,o,"PLL Prescaler","1:1","1:2");
	rb(1,x,o,"PLL","ON","OFF");
	if(x&2) { plllock_at=clocks+250; } else { x=x&0xfe; }
	mem[a]=x;
}

#include "io.inc"

/**************************************************************/

void setreg(unsigned int r, uint8_t v) {
	assert(r<0x20);
	mem[r]=v;
	LOG("  r%u=%02x\n",r,v);
}

void setreg16(unsigned int r, uint16_t v) {
	assert(r<0x1f);
	mem[r]=v;
	mem[r+1]=v>>8;
	LOG("  r%u+1:%u=%04x\n",r,r,v);
}

void setio(unsigned int addr, uint8_t v) {
	assert(addr>0x1f&&addr<0x100);
	if(io_write_funcs[addr-0x20]) {
		io_write_funcs[addr-0x20](addr,v);
	} else {
		printf("io write %04x=0x%02x is not implemented\n",addr,v);
		abort();
	}
}

void setmem(unsigned int addr,uint8_t v) {
	if(addr<0x20) { setreg(addr,v); }
	else if(addr<0x100) { setio(addr,v); }
	else if(addr<0x4000) {
		mem[addr]=v;
		LOG("  [%04x]=%02x\n",addr,v);
	} else {
		printf("address [%04x]=%02x above available memory\n",addr,v);
		abort();
	}
}

void setZ(int x) { mem[0x5f]&=~0x02; if(x) mem[0x5f]|=0x02; }
void setC(int x) { mem[0x5f]&=~0x01; if(x) mem[0x5f]|=0x01; }
void setNV(int n,int v) {
	mem[0x5f]&=~0x04; if(n) mem[0x5f]|=0x04;
	mem[0x5f]&=~0x08; if(v) mem[0x5f]|=0x08;
	if((n||v)&&(!(n&&v))) {
		mem[0x5f]&=~0x10; if(n) mem[0x5f]|=0x10;
	}
}

void setsp(uint16_t x) {
	mem[0x5e]=x>>8;
	mem[0x5d]=x&0xff;
}

/**************************************************************/

uint16_t sp() {
	return (mem[0x5e]<<8)|mem[0x5d];
}

uint8_t reg(unsigned int x) {
	assert(x<0x20);
	return mem[x];
}

uint16_t reg16(unsigned int x) {
	assert(x<0x1f);
	return (mem[x+1]<<8)|mem[x];
}


uint8_t io(unsigned int addr) {
	assert(addr>0x1f&&addr<0x100);
	if(io_read_funcs[addr-0x20]) {
		return io_read_funcs[addr-0x20](addr);
	} else {
		printf("io read %04x is not implemented\n",addr);
		abort();
	}
}

uint8_t getmem(unsigned int addr) {
	if(addr<0x20) { return reg(addr); }
	else if(addr<0x100) { return io(addr); }
	else if(addr<0x4000) {
		return mem[addr];
	} else {
		printf("address [%04x] above available memory\n",addr);
		abort();
	}
}

int getC() { return mem[0x5f]&1; }
int getZ() { return mem[0x5f]&2; }


/**************************************************************/

#ifdef VERBOSE
void dumpSREG() {
	uint8_t s=mem[0x5f];
	LOG("  SREG:%c%c%c%c%c%c%c%c\n",
		(s&0x80)?'I':'_',
		(s&0x40)?'T':'_',
		(s&0x20)?'H':'_',
		(s&0x10)?'S':'_',
		(s&0x08)?'V':'_',
		(s&0x04)?'N':'_',
		(s&0x02)?'Z':'_',
		(s&0x01)?'C':'_');
}
#else
#define dumpSREG() ;
#endif

/**************************************************************/

typedef int (*avr_inst)(uint16_t);

#include "args.inc"

int avr_LDI(uint16_t i) {
        setreg(0x10+ARG_LDI_B,ARG_LDI_A);
        return 1;
}

int avr_STS(uint16_t i) {
	setmem(flash[pc],reg(ARG_STS_A));
        return 2;
}

int avr_SBI(uint16_t i) {
	setmem(ARG_SBI_A+0x20,mem[ARG_SBI_A+0x20]|(1<<ARG_SBI_B));
	return 2;
}

int avr_CBI(uint16_t i) {
	setmem(ARG_SBI_A+0x20,mem[ARG_SBI_A+0x20]&(~(1<<ARG_SBI_B)));
	return 2;
}

int avr_NOP(uint16_t i) {
	return 1;
}

int avr_DEC(uint16_t i) {
	int r=ARG_DEC_A;
	uint8_t v;
        setreg(r,v=reg(r)-1);
        
        setZ(v==0);
	setNV(v&0x80,v==0x7f);
	dumpSREG();

	return 1;
}

int avr_RCALL(uint16_t i) {
        uint16_t p=sp();
        mem[p]=pc&0xff;
        mem[p-1]=pc>>8;
        setsp(p-2);
        pc+=ARG_RCALL_A;
        return 3;
}

int avr_BRNE(uint16_t i) {
	dumpSREG();
        if (!getZ()) {
                pc+=ARG_BRNE_A;
                return 2;
	}
        return 1;
}

int avr_RJMP(uint16_t i) {
	pc+=ARG_RJMP_A;
	return 2;
}

int avr_RET(uint16_t i) {
        uint16_t p=sp();
        pc=(mem[p+1]<<8)|mem[p+2];
        setsp(p+2);
        return 4;
}

int avr_EOR(uint16_t i) {
	int rd=ARG_EOR_B;
	uint8_t r;
	setreg(rd,r=(reg(rd)^reg(ARG_EOR_A)));
	setZ(r==0);
	setNV(r&0x80,0);
	return 1;
}

int avr_OUT(uint16_t i) {
	setmem(0x20+ARG_OUT_A,reg(ARG_OUT_B));
	return 1;
}

int avr_IN(uint16_t i) {
	setreg(ARG_OUT_B,io(0x20+ARG_OUT_A));
	return 1;
}

int avr_CPI(uint16_t i) {
	uint8_t d=reg(0x10+ARG_CPI_B);
	uint8_t k=ARG_CPI_A;
	int u=d-k;
	int s=(int8_t)d-(int8_t)k;

	setZ((u&0xff)==0);
	setNV(u&0x80,s<-128||s>128);
	setC(u<0||u>255);

	return 1;
}

int avr_CPC(uint16_t i) {
	int c=getC();
	uint8_t d=reg(ARG_CPC_B);
	uint8_t r=reg(ARG_CPC_A);
	int u=d-r-c;
	int s=(int8_t)d-(int8_t)r-c;

	setZ((u&0xff)==0&&getZ());
	setNV(u&0x80,s<-128||s>128);
	setC(u<0||u>255);

	return 1;
}

int avr_LPMZP(uint16_t i) {
	uint16_t a=reg16(0x1e);
	setreg(ARG_LPMZP_A,flash[a/2]>>((a&1)?8:0));
	setreg16(0x1e,a+1);
	return 3;
}

int avr_STXP(uint16_t i) {
	uint16_t a=reg16(0x1a);
	setmem(a,reg(ARG_STXP_A));
	setreg16(0x1a,a+1);
	return 2;
}

int avr_PUSH(uint16_t i) {
	uint16_t s=sp();
	mem[s]=reg(ARG_PUSH_A);
	setsp(s-1);
	return 2;
}

int avr_SBRS(uint16_t i) {
	if(reg(ARG_SBRS_A)&(1<<ARG_SBRS_B)) {
		skipnext=1; return 2;
	}
	return 1;
}

#define avr_UNIMPL (0)
#define avr_LDS avr_UNIMPL
#define avr_MOVW avr_UNIMPL
#define avr_LPMZ avr_UNIMPL
#define avr_LDZP avr_UNIMPL
#define avr_LDX avr_UNIMPL
#define avr_LDDZ avr_UNIMPL
#define avr_STZP avr_UNIMPL
#define avr_STX avr_UNIMPL
#define avr_STY avr_UNIMPL
#define avr_STZ avr_UNIMPL
#define avr_STDY avr_UNIMPL
#define avr_STDZ avr_UNIMPL
#define avr_ANDI avr_UNIMPL
#define avr_ORI avr_UNIMPL
#define avr_SBCI avr_UNIMPL
#define avr_ADC avr_UNIMPL
#define avr_POP avr_UNIMPL
#define avr_LSR avr_UNIMPL
#define avr_ROR avr_UNIMPL
#define avr_AND avr_UNIMPL
#define avr_ADD avr_UNIMPL
#define avr_SUB avr_UNIMPL
#define avr_SUBI avr_UNIMPL
#define avr_STS16 avr_UNIMPL
#define avr_SBIW avr_UNIMPL
#define avr_MOV avr_UNIMPL
#define avr_OR avr_UNIMPL
#define avr_CP avr_UNIMPL
#define avr_ADIW avr_UNIMPL
#define avr_SBRC avr_UNIMPL
#define avr_BRCS avr_UNIMPL
#define avr_BREQ avr_UNIMPL
#define avr_BRGE avr_UNIMPL
#define avr_BRCC avr_UNIMPL
#define avr_BRPL avr_UNIMPL
#define avr_RETI avr_UNIMPL
#define avr_SEC avr_UNIMPL
#define avr_SEI avr_UNIMPL
#define avr_CLI avr_UNIMPL

/**************************************************************/

#include "decode.inc"

uint64_t time64() {
	struct timeval t;
	gettimeofday(&t,0);
	return (uint64_t)t.tv_sec*1000000+t.tv_usec;
}

void run() {
	uint64_t start_report=time64();
	uint64_t clock_report=clocks;

	uint64_t delay_clock=0;

	struct timespec delay={0,1};

	for(;;) {
		int clock_divisor=1<<(mem[0x61]&0xf);
		LOG("%4x (%lu):",pc*2,clocks);

		if(clocks-clock_report>=8000000/clock_divisor) {
			uint64_t now=time64();
			printf("## %lu clocks per %lu usecond\n",(clocks-clock_report),(now-start_report));
			int clocks_passed=clocks-clock_report;
			int us_passed=now-start_report;


			int fc=(1000/8)*clock_divisor-(us_passed*1000)/clocks_passed;
			printf("need to slowdown for %i ns per clock\n",fc);
			delay.tv_nsec+=fc*1000;
			if(delay.tv_nsec<0) delay.tv_nsec=0;

			start_report=now; clock_report=clocks;
		}

		uint16_t i=flash[pc];
		int f=decode(i);
		if(f==INST_UNKNOWN) {
			printf("unknown instruction %04x at %04x\n",i,pc*2);
			char buf[1024];
			sprintf(buf,"grep '^%4x' disasm.txt",pc*2);
			if(system(buf)) ;
			return;

		} else if(f<0) {
			f=-f;
			pc++;
			if(skipnext) clocks++;
		}

		LOG("%s\n",inst_names[f]);
		if(skipnext) {
			LOG("  skipped\n");
			skipnext=0;
		} else if(inst_funcs[f]) {
			int clocksdone=inst_funcs[f](i);
			clocks+=clocksdone;
			delay_clock+=clocksdone;
			if(delay_clock>1000 && delay.tv_nsec) {
				nanosleep(&delay,0);
				delay_clock=0;
			}
		} else {
			printf("instruction %s (%04x at %04x) not implemented\n",inst_names[f],i,pc*2);
			char buf[1024];
			sprintf(buf,"grep '^%4x' disasm.txt",pc*2);
			if(system(buf)) ;
			return;
		}
		pc++;

		schedules();
	}
}

void reset() {
	setsp(0xaff);
	pc=0;
	clocks=0;
	skipnext=0;
}

int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	load(argv[1]);
	reset();
	run();
	return 0;
}

