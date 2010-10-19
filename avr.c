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
		pc++;
	}
}

int main() {
	load();
	reset();
	run();
	return 0;
}

