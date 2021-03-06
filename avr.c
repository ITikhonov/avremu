#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

#include <stdio.h>

#undef NDEBUG
#include <assert.h>

extern void usb_poll(uint64_t d);

int gfs_init();
int gfs_poll(uint64_t ns, uint8_t *r);

uint8_t setup_pkt[8];

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

void rb0(int bit,uint8_t n,char *name,char *on, char *off) {
	int m=1<<bit;
	printf("  %s: %s\n",name,(n&m)?on:off);
}

void rl(int bit,uint8_t n,char *name) {
	int m=1<<bit;
	if(n&m) {
		printf("  %s\n",name);
	}
}


/**************************************************************/

uint64_t plllock_at=0;

typedef void (*sched_func)(void);

struct {
	uint64_t clock;
	sched_func f;
} sched[10]={{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};

void schedule(uint64_t after, sched_func f) {
	int i;
	for(i=0;i<10;i++) {
		if(sched[i].clock) continue;
		sched[i].clock=clocks+after;
		sched[i].f=f;
		return;
	}
	printf("No free schedulers\n");
	abort();
}

void schedules() {
	int i;
	for(i=0;i<10;i++) {
		if(sched[i].clock && sched[i].clock<clocks) {
			sched[i].clock=0;
			sched[i].f();
		}
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

#undef avr_IOR5f
uint8_t avr_IOR5f(uint8_t a) { return mem[a]; }
#undef avr_IOW5f
void avr_IOW5f(uint8_t a,uint8_t x) { mem[a]=x; }

#undef avr_IOR5e
#undef avr_IOW5e
uint8_t avr_IOR5e(uint8_t a) { return mem[a]; }
void avr_IOW5e(uint8_t a,uint8_t x) { mem[a]=x; }

#undef avr_IOR5d
#undef avr_IOW5d
uint8_t avr_IOR5d(uint8_t a) { return mem[a]; }
void avr_IOW5d(uint8_t a,uint8_t x) { mem[a]=x; }

#undef avr_IOW61
void avr_IOW61(uint8_t a,uint8_t x) {
	if(x==0x80) { printf("  CLKPCE\n"); }
	else { printf("  CLKPS:%x\n",1<<(x&0xf)); }
	mem[a]=x;
}

#undef avr_IOR49
uint8_t avr_IOR49(uint8_t a) {
	return mem[a];
}


void plllock() {
	printf("!! PLLLOCK\n");
	mem[0x49]|=1;
}

#undef avr_IOW49
void avr_IOW49(uint8_t a,uint8_t x) {
	uint8_t o=mem[a];
	x=(x&0xfe)|(o&1);

	rb(4,x,o,"PLL Prescaler","1:1","1:2");
	rb(1,x,o,"PLL","ON","OFF");
	if(x&2) { schedule(250,plllock); } else { x=x&0xfe; }
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
	else if(addr<0xb00) {
		mem[addr]=v;
		LOG("  [%04x]=%02x\n",addr,v);
	} else {
		printf("address [%04x]=%02x above available memory\n",addr,v);
		abort();
	}
}

int getI(int x) { return (mem[0x5f]&0x80)?1:0; }
void setI(int x) { mem[0x5f]&=~0x80; if(x) mem[0x5f]|=0x80; }
void setZ(int x) { mem[0x5f]&=~0x02; if(x) mem[0x5f]|=0x02; }
void setC(int x) { mem[0x5f]&=~0x01; if(x) mem[0x5f]|=0x01; }
void setNV(int n,int v) {
	mem[0x5f]&=~0x04; if(n) mem[0x5f]|=0x04;
	mem[0x5f]&=~0x08; if(v) mem[0x5f]|=0x08;
	if((n||v)&&(!(n&&v))) {
		mem[0x5f]&=~0x10; if(n) mem[0x5f]|=0x10;
	}
}


static int borrow(uint8_t R, uint8_t d, uint8_t r, int b)
{
    uint8_t Rb = R >> b & 0x1;
    uint8_t db = d >> b & 0x1;
    uint8_t rb = r >> b & 0x1;
    return (~db & rb) | (rb & Rb) | (Rb & ~db);
}

static void set_borrow(uint8_t R, uint8_t d, uint8_t r) {
	setC(borrow(R,d,r,7));
}


static int sub_overflow (uint8_t R, uint8_t d, uint8_t r)
{
    uint8_t R7 = R >> 7 & 0x1;
    uint8_t d7 = d >> 7 & 0x1;
    uint8_t r7 = r >> 7 & 0x1;
    return (d7 & ~r7 & ~R7) | (~d7 & r7 & R7);
}

void setNV_sub(uint8_t R, uint8_t d, uint8_t r) {
	setNV(R&0x80,sub_overflow(R,d,r));
}

static int carry(uint8_t R, uint8_t d, uint8_t r, int b)
{
    uint8_t Rb = R >> b & 0x1;
    uint8_t db = d >> b & 0x1;
    uint8_t rb = r >> b & 0x1;
    return (db & rb) | (rb & ~Rb) | (~Rb & db);
}

static void set_carry(uint8_t R, uint8_t d, uint8_t r) {
	setC(carry(R,d,r,7));
}

static int add_overflow (uint8_t R, uint8_t d, uint8_t r)
{
    uint8_t R7 = R >> 7 & 0x1;
    uint8_t d7 = d >> 7 & 0x1;
    uint8_t r7 = r >> 7 & 0x1;
    return (d7 & r7 & ~R7) | (~d7 & ~r7 & R7);
}


void setNV_add(uint8_t R, uint8_t d, uint8_t r) {
	setNV(R&0x80,add_overflow(R,d,r));
}

void setsp(uint16_t x) {
	mem[0x5e]=x>>8;
	mem[0x5d]=x&0xff;
}

uint16_t sp();

void push16(uint16_t x) {
        uint16_t p=sp();
        mem[p]=x&0xff;
        mem[p-1]=x>>8;
        setsp(p-2);
}

/**************************************************************/

uint16_t sp() {
	return (mem[0x5e]<<8)|mem[0x5d];
}

uint16_t pop16() {
        uint16_t p=sp();
        setsp(p+2);
	return (mem[p+1]<<8)|mem[p+2];
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

uint16_t getflash(unsigned int x) {
	if(x<0x4000) return flash[x];
	printf("flash [%04x] above available memory\n",x);
	abort();
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
	push16(pc);
        pc+=ARG_RCALL_A;
        return 3;
}

int avr_BRNE(uint16_t i) {
	dumpSREG();
        if (!getZ()) { pc+=ARG_BRNE_A; return 2; }
        return 1;
}

int avr_BRCS(uint16_t i) {
	dumpSREG();
        if (getC()) { pc+=ARG_BRCS_A; return 2; }
        return 1;
}


int avr_BRCC(uint16_t i) {
	dumpSREG();
        if (!getC()) { pc+=ARG_BRCC_A; return 2; }
        return 1;
}

int avr_BREQ(uint16_t i) {
	dumpSREG();
        if (getZ()) { pc+=ARG_BREQ_A; return 2; }
        return 1;
}

int avr_RJMP(uint16_t i) {
	pc+=ARG_RJMP_A;
	return 2;
}

int avr_RET(uint16_t i) {
        pc=pop16();
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

	printf("  CPI %02x-%02x\n",d,k);

	setZ((u&0xff)==0);
	setNV_sub(u,d,k);
	set_borrow(u,d,k);

	return 1;
}

int avr_CPC(uint16_t i) {
	int c=getC();
	uint8_t d=reg(ARG_CPC_B);
	uint8_t r=reg(ARG_CPC_A);

	printf("  CPC %02x-%02x-%u\n",d,r,c);

	int u=d-r-c;

	setZ((u&0xff)==0&&getZ());
	setNV_sub(u,d,r+c);
	set_borrow(u,d,r+c);
	dumpSREG();

	return 1;
}

int avr_CP(uint16_t i) {
	uint8_t d=reg(ARG_CPC_B);
	uint8_t r=reg(ARG_CPC_A);
	int u=d-r;

	printf("  CP %02x-%02x\n",d,r);

	setZ((u&0xff)==0);
	setNV_sub(u,d,r);
	set_borrow(u,d,r);
	dumpSREG();

	return 1;
}


int avr_LPMZP(uint16_t i) {
	uint16_t a=reg16(0x1e);
	setreg(ARG_LPMZP_A,flash[a/2]>>((a&1)?8:0));
	setreg16(0x1e,a+1);
	return 3;
}

int avr_LPMZ(uint16_t i) {
	uint16_t a=reg16(0x1e);
	setreg(ARG_LPMZ_A,flash[a/2]>>((a&1)?8:0));
	return 3;
}

int avr_STXP(uint16_t i) {
	uint16_t a=reg16(0x1a);
	setmem(a,reg(ARG_STXP_A));
	setreg16(0x1a,a+1);
	return 2;
}

int avr_STZP(uint16_t i) {
	uint16_t a=reg16(0x1e);
	setmem(a,reg(ARG_STZP_A));
	setreg16(0x1e,a+1);
	return 2;
}

int avr_PUSH(uint16_t i) {
	uint16_t s=sp();
	mem[s]=reg(ARG_PUSH_A);
	setsp(s-1);
	return 2;
}

int avr_POP(uint16_t i) {
	uint16_t s=sp();
	setreg(ARG_POP_A,mem[s+1]);
	setsp(s+1);
	return 2;
}

int avr_SBRS(uint16_t i) {
	if(reg(ARG_SBRS_A)&(1<<ARG_SBRS_B)) {
		skipnext=1; return 2;
	}
	return 1;
}


int avr_SBRC(uint16_t i) {
	if(!(reg(ARG_SBRS_A)&(1<<ARG_SBRS_B))) {
		skipnext=1; return 2;
	}
	return 1;
}

int avr_SEI(uint16_t i) {
	setI(1);
	return 1;
}


int avr_CLI(uint16_t i) {
	setI(0);
	return 1;
}

int avr_LDS(uint16_t i) {
	setreg(ARG_LDS_A,getmem(getflash(pc)));
	return 2;
}

int avr_ANDI(uint16_t i) {
	uint8_t r;
	int d=ARG_ANDI_B+0x10;
	setreg(d,r=reg(d)&ARG_ANDI_A);
	setZ(r==0);
	setNV(r&0x80,0);
	return 1;
}


int avr_ORI(uint16_t i) {
	uint8_t r;
	int d=ARG_ORI_B+0x10;
	setreg(d,r=reg(d)|ARG_ORI_A);
	setZ(r==0);
	setNV(r&0x80,0);
	return 1;
}


int avr_AND(uint16_t i) {
	uint8_t r;
	int d=ARG_AND_B;
	setreg(d,r=reg(d)&reg(ARG_AND_A));
	setZ(r==0);
	setNV(r&0x80,0);
	return 1;
}

int avr_MOV(uint16_t i) {
	setreg(ARG_MOV_B,reg(ARG_MOV_A));
	return 1;
}

int avr_RETI(uint16_t i) {
	setI(1);
        pc=pop16()-1; // it will be pc+1'ed in run
        return 4;
}


int avr_OR(uint16_t i) {
	uint8_t r;
	int d=ARG_OR_B;
	setreg(d,r=reg(d)|reg(ARG_OR_A));
	setZ(r==0);
	setNV(r&0x80,0);
	return 1;
}

int avr_MOVW(uint16_t i) {
	setreg16(ARG_MOVW_A*2,reg16(ARG_MOVW_B*2));
	return 1;
}

int avr_ADC(uint16_t i) {
	int c=getC();
	uint8_t rd=ARG_ADD_B;
	uint8_t d=reg(rd);
	uint8_t r=reg(ARG_ADD_A);
	int u=d+r+c;

	setreg(rd,u);
	setZ((u&0xff)==0);
	setNV_add(u,d,r+c);
	set_carry(u,d,r+c);
	return 1;
}

int avr_ADD(uint16_t i) {
	uint8_t rd=ARG_ADD_B;
	uint8_t d=reg(rd);
	uint8_t r=reg(ARG_ADD_A);
	int u=d+r;

	setreg(rd,u);
	setZ((u&0xff)==0);
	setNV_add(u,d,r);
	set_carry(u,d,r);
	return 1;
}


int avr_SUB(uint16_t i) {
	uint8_t rd=ARG_SUB_B;
	uint8_t d=reg(rd);
	uint8_t r=reg(ARG_SUB_A);
	int u=d-r;

	setreg(rd,u);
	setZ((u&0xff)==0);
	setNV_sub(u,d,r);
	set_borrow(u,d,r);

	return 1;
}

int avr_SUBI(uint16_t i) {
	uint8_t r=0x10+ARG_SUBI_B;
	uint8_t d=reg(r);
	uint8_t k=ARG_SUBI_A;
	int u=d-k;

	setreg(r,u);
	setZ((u&0xff)==0);
	setNV_sub(u,d,k);
	set_borrow(u,d,k);

	return 1;
}

int avr_SBCI(uint16_t i) {
	int c=getC();
	int rd=ARG_SBCI_B+0x10;
	uint8_t d=reg(rd);
	uint8_t k=ARG_SBCI_A;
	int u=d-k-c;
	
	setreg(rd,u);
	setZ((u&0xff)==0&&getZ());
	setNV_sub(u,d,k+c);
	set_borrow(u,d,k+c);

	return 1;
}

int avr_SBIW(uint16_t i) {
	int rd=ARG_SBIW_B*2+24;
	printf("  SBIW rd=%u (%u)\n",rd,ARG_SBIW_B);
	uint16_t d=reg16(rd);
	uint8_t k=ARG_SBIW_A;
	uint16_t u=d-k;
	
	setreg16(rd,u);
	setZ(u==0);
	setNV(u&0x8000,(d&0x8000)&&!(u&0x8000));
	setC(u&0x8000&&!(d&0x8000));

	return 2;
}

int avr_ADIW(uint16_t i) {
	int rd=ARG_ADIW_B*2+24;
	uint16_t d=reg16(rd);
	uint8_t k=ARG_ADIW_A;
	uint16_t u=d+k;

	setreg16(rd,u);
	setZ(u==0);
	setNV(u&0x8000,(u&0x8000)&&!(d&0x8000));
	setC(d&0x8000&&!(u&0x8000));

	return 2;
}




#define avr_UNIMPL (0)
#define avr_LDZP avr_UNIMPL
#define avr_LDX avr_UNIMPL
#define avr_LDDZ avr_UNIMPL
#define avr_STX avr_UNIMPL
#define avr_STY avr_UNIMPL
#define avr_STZ avr_UNIMPL
#define avr_STDY avr_UNIMPL
#define avr_STDZ avr_UNIMPL
#define avr_LSR avr_UNIMPL
#define avr_ROR avr_UNIMPL
#define avr_STS16 avr_UNIMPL
#define avr_BRGE avr_UNIMPL
#define avr_BRPL avr_UNIMPL
#define avr_SEC avr_UNIMPL

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

		} else {
			printf("instruction %s (%04x at %04x) not implemented\n",inst_names[f],i,pc*2);
			char buf[1024];
			sprintf(buf,"grep '^%4x' disasm.txt",pc*2);
			if(system(buf)) ;
			return;
		}
		pc++;

		if(delay_clock>1000 && delay.tv_nsec) {
			nanosleep(&delay,0);
			delay_clock=0;
		}

		usb_poll(0);
		schedules();
	}
}

void intr(uint8_t a) {
        push16(pc);
        setI(0);
        pc=a;
}

void add_ior(uint8_t a,io_read_func f) {
	printf("add ior for %x\n",a);
	io_read_funcs[a-0x20]=f;
}
void add_iow(uint8_t a,io_write_func f) {
	printf("add ior for %x\n",a);
	io_write_funcs[a-0x20]=f;
}


void reset() {
	setsp(0xaff);
	mem[0xd8]=0b00100000; // USBCON
	pc=0;
	clocks=0;
	skipnext=0;

}

void usb_init();

int main(int argc, char *argv[]) {
	setlinebuf(stdout);
	usb_init();

	load(argv[1]);
	reset();
	run();
	return 0;
}

