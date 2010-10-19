#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>

#undef NDEBUG
#include <assert.h>

uint8_t mem[0xb00];
uint16_t flash[0x4000];
uint16_t pc;

typedef void (*io_read_func)(uint8_t a);
typedef void (*io_write_func)(uint8_t a,uint8_t x);

void load() {
	int f=open("blink_slow.bin",O_RDONLY);
	read(f,&flash,sizeof(flash));
}

void reset() {
	pc=0;
}

/**************************************************************/

#include "ioundef.inc"

#undef avr_IOW61
void avr_IOW61(uint8_t a,uint8_t x) {
	if(x==0x80) { printf("  CLKPCE\n"); }
	else { printf("  CLKPS:%x\n",1<<(x&0xf)); }
	mem[0x61]=x;
}

#include "io.inc"

/**************************************************************/

void setreg(unsigned int r, uint8_t v) {
	assert(r<0x20);
	mem[r]=v;
	printf("  r%u=%02x\n",r,v);
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
		printf("  [%04x]=%02x\n",addr,v);
	} else {
		printf("address [%04x]=%02x above available memory\n",addr,v);
		abort();
	}
}

/**************************************************************/

uint8_t reg(unsigned int x) {
	assert(x<0x20);
	return mem[x];
}

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

#define avr_UNIMPL (0)
#define avr_LDS avr_UNIMPL
#define avr_SBI avr_UNIMPL
#define avr_CBI avr_UNIMPL
#define avr_RCALL avr_UNIMPL
#define avr_RJMP avr_UNIMPL
#define avr_DEC avr_UNIMPL
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
#define avr_BRNE avr_UNIMPL
#define avr_BRCS avr_UNIMPL
#define avr_BREQ avr_UNIMPL
#define avr_BRGE avr_UNIMPL
#define avr_BRCC avr_UNIMPL
#define avr_BRPL avr_UNIMPL
#define avr_RET avr_UNIMPL
#define avr_RETI avr_UNIMPL
#define avr_SEC avr_UNIMPL
#define avr_SEI avr_UNIMPL
#define avr_CLI avr_UNIMPL
#define avr_NOP avr_UNIMPL

#include "decode.inc"

void run() {
	for(;;) {
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
		printf("%s\n",inst_names[f]);
		if(inst_funcs[f]) {
			inst_funcs[f](i);
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

int main() {
	load();
	reset();
	run();
	return 0;
}

