#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdarg.h>

#include <stdio.h>

#undef NDEBUG
#include <assert.h>

uint8_t mem[0xb00];
uint16_t flash[0x4000];
uint16_t pc;
uint32_t clocks;

typedef void (*io_read_func)(uint8_t a);
typedef void (*io_write_func)(uint8_t a,uint8_t x);

void load() {
	int f=open("blink_slow.bin",O_RDONLY);
	read(f,&flash,sizeof(flash));
}

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

#undef avr_IOW61
void avr_IOW61(uint8_t a,uint8_t x) {
	if(x==0x80) { printf("  CLKPCE\n"); }
	else { printf("  CLKPS:%x\n",1<<(x&0xf)); }
	mem[a]=x;
}

#include "io.inc"

/**************************************************************/

void setreg(unsigned int r, uint8_t v) {
	assert(r<0x20);
	mem[r]=v;
	LOG("  r%u=%02x\n",r,v);
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

#define avr_UNIMPL (0)
#define avr_LDS avr_UNIMPL
#define avr_MOVW avr_UNIMPL
#define avr_OUT avr_UNIMPL
#define avr_LPMZP avr_UNIMPL
#define avr_LPMZ avr_UNIMPL
#define avr_LDZP avr_UNIMPL
#define avr_LDX avr_UNIMPL
#define avr_LDDZ avr_UNIMPL
#define avr_STXP avr_UNIMPL
#define avr_STZP avr_UNIMPL
#define avr_STX avr_UNIMPL
#define avr_STY avr_UNIMPL
#define avr_STZ avr_UNIMPL
#define avr_STDY avr_UNIMPL
#define avr_STDZ avr_UNIMPL
#define avr_CPI avr_UNIMPL
#define avr_ANDI avr_UNIMPL
#define avr_ORI avr_UNIMPL
#define avr_SBCI avr_UNIMPL
#define avr_CPC avr_UNIMPL
#define avr_ADC avr_UNIMPL
#define avr_PUSH avr_UNIMPL
#define avr_POP avr_UNIMPL
#define avr_LSR avr_UNIMPL
#define avr_ROR avr_UNIMPL
#define avr_IN avr_UNIMPL
#define avr_EOR avr_UNIMPL
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
#define avr_SBRS avr_UNIMPL
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

double floattime() {
	struct timeval t;
	gettimeofday(&t,0);
	return t.tv_sec+(t.tv_usec*1e-6);
}

void run() {
	double start=floattime();
	for(;;) {
		LOG("%4x (%u):",pc*2,clocks);
		if(clocks==8000000) {
			double now=floattime();
			printf("## %u clocks per %0.3lf seconds\n",clocks,now-start);
			start=now; clocks=0;
		}

		uint16_t i=flash[pc];
		int f=decode(i);
		if(f==INST_UNKNOWN) {
			printf("unknown instruction %04x at %04x\n",i,pc*2);
			char buf[1024];
			sprintf(buf,"grep '^%4x' disasm.txt",pc*2);
			system(buf);
			return;

		} else if(f<0) {
			f=-f;
			pc++;
		}
		LOG("%s\n",inst_names[f]);
		if(inst_funcs[f]) {
			clocks+=inst_funcs[f](i);
		} else {
			printf("instruction %s (%04x at %04x) not implemented\n",inst_names[f],i,pc*2);
			char buf[1024];
			sprintf(buf,"grep '^%4x' disasm.txt",pc*2);
			system(buf);
			return;
		}
		pc++;
	}
}

void reset() {
	setsp(0xaff);
	pc=0;
	clocks=0;
}

int main() {
	load();
	reset();
	run();
	return 0;
}

