#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>

uint8_t mem[0xb00];
uint16_t flash[0x4000];
uint16_t pc;

void load() {
	int f=open("blink_slow.bin",O_RDONLY);
	read(f,&flash,sizeof(flash));
}

void reset() {
	pc=0;
}

void setreg(int r, uint8_t v) {
	mem[r]=v;
	printf("  r%u=%02x\n",r,v);
}

typedef int (*avr_inst)(uint16_t);

#include "args.inc"

int avr_LDI(uint16_t i) {
        setreg(0x10+ARG_LDI_B,ARG_LDI_A);
        return 1;
}

#define avr_UNIMPL (0)
#define avr_STS avr_UNIMPL
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

