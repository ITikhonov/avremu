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

#include "common.h"
#include "qemu.h"

static uint8_t UDINT; // 0xe1
static uint8_t UENUM; // 0xe9
static uint8_t UDADDR; // 0xe3

static enum {SETUP,IN,OUT} usbpid;

static struct {
	uint8_t UEINTX; // e8
	uint8_t UECONX; // eb
	uint8_t UECFG0X; // ec
	uint8_t UECFG1X; // ed
	uint8_t UEIENX; // f0

	uint8_t fifo[64];
	int fifoi;
} ep[6];

static void sim_usb_initspeed() {
	printf("!! USB INIT SPEED\n");
	qemu_attach();
	UDINT=1<<3;
	intr(0x14);
}


static void sim_operate() {
	int ret,e;
	if((ret=qemu_poll(1,ep[0].fifo,&e))) {
		if(e>5) {
			printf("We have only endpoints 0 to 5, but got %u\n",e);
			abort();
		}
		if(ret==1) {
			ep[0].fifoi=0;
			usbpid=SETUP;

			int i;
			printf("RX:");
			for(i=0;i<8;i++) { printf(" %02x",ep[0].fifo[i]); }
			printf("\n");

			ep[0].UEINTX|=1<<3;
		} else if(ret==2) {
			ep[e].UEINTX|=1;
		} else if(ret==3) {
			ep[e].UEINTX|=1<<2;
		}
	}

	
	if(getI()) {
		if (ep[0].UEINTX&ep[0].UEIENX) { intr(0x16); return; }
		if (ep[1].UEINTX&ep[1].UEIENX) { intr(0x16); return; }
		if (ep[2].UEINTX&ep[2].UEIENX) { intr(0x16); return; }
		if (ep[3].UEINTX&ep[3].UEIENX) { intr(0x16); return; }
		if (ep[4].UEINTX&ep[4].UEIENX) { intr(0x16); return; }
		if (ep[5].UEINTX&ep[5].UEIENX) { intr(0x16); return; }
	}
}

static void sim_gfs_write() {
	qemu_write(ep[UENUM].fifo,ep[UENUM].fifoi);
	ep[UENUM].fifoi=0;
}

static uint8_t avr_IORe1(uint8_t a) { return UDINT; }
static void avr_IOWe1(uint8_t a, uint8_t x) { UDINT=x; }

// UEDATX
static uint8_t avr_IORf1(uint8_t a) {
	uint8_t x=ep[UENUM].fifo[ep[UENUM].fifoi++];
	printf("  USB READ EP%u: %02x\n",UENUM,x);
	return x;
}

static void avr_IOWf1(uint8_t a, uint8_t x) {
	ep[UENUM].fifo[ep[UENUM].fifoi++]=x;
	printf("  USB WRITE EP%u: %02x\n",UENUM,x);
}

static void avr_IOWe9(uint8_t a,uint8_t x) {
	printf("  USB SELECT: EP%u\n", x&7);
	UENUM=x;
}


static void avr_IOWeb(uint8_t a,uint8_t x) {
	rb0(0,x,"USB EP ENABLED","ON","OFF");
	rl(3,x,"USB DATA TOGGLE RESET");
	rl(4,x,"USB DISABLE STALL");
	rl(5,x,"USB STALL REQUEST");

	ep[UENUM].UECONX=x;
}

static void avr_IOWec(uint8_t a,uint8_t x) {
	rb0(0,x,"USB EP DIRECTION","IN","OUT");
	char *ept[4]={"CONTROL","ISOCHRONOUS","BULK","INTERRUPT"};
	printf("  USB EP TYPE: %s\n",ept[x>>6]);
	ep[UENUM].UECFG0X=x;
}

static void avr_IOWed(uint8_t a,uint8_t x) {
	rb0(1,x,"USB EP MEMORY","ALLOC","FREE");
	rb0(2,x,"USB EP BANKS","2","1");
	printf("  USB EP SIZE: %u\n",8<<((x>>4)&7));
	ep[UENUM].UECFG1X=x;
}


static void avr_IOWea(uint8_t a,uint8_t x) {
	rb0(0,x,"USB EP0 RST","ON","OFF");
	rb0(1,x,"USB EP1 RST","ON","OFF");
	rb0(2,x,"USB EP2 RST","ON","OFF");
	rb0(3,x,"USB EP3 RST","ON","OFF");
	rb0(4,x,"USB EP4 RST","ON","OFF");
	rb0(5,x,"USB EP5 RST","ON","OFF");
	rb0(6,x,"USB EP6 RST","ON","OFF");
	rb0(7,x,"USB EP7 RST","ON","OFF");
}

static void avr_IOWf0(uint8_t a,uint8_t x) {
	rb0(0,x,"USB EP TXINE","ON","OFF");
	rb0(1,x,"USB EP STALLEDE","ON","OFF");
	rb0(2,x,"USB EP RXOUTE","ON","OFF");
	rb0(3,x,"USB EP RXSTPE","ON","OFF");
	rb0(4,x,"USB EP NAKOUTE","ON","OFF");
	rb0(6,x,"USB EP NAKINE","ON","OFF");
	rb0(7,x,"USB EP FLERRE","ON","OFF");

	ep[UENUM].UEIENX=x;
}

static uint8_t avr_IORe8(uint8_t a) {
	printf("  READ EP%u UEINTX=%02x\n",UENUM,ep[UENUM].UEINTX);
	return ep[UENUM].UEINTX;
}

static void avr_IOWe8(uint8_t a, uint8_t x) {
	rb0(0,x,"USB EP TXINI","ON","OFF");
	rb0(1,x,"USB EP STALLEDI","ON","OFF");
	rb0(2,x,"USB EP RXOUTI","ON","OFF");
	rb0(3,x,"USB EP RXSTPI","ON","OFF");
	rb0(4,x,"USB EP NAKOUTI","ON","OFF");
	rb0(5,x,"USB EP RWAL","ON","OFF");
	rb0(6,x,"USB EP NAKINI","ON","OFF");
	rb0(7,x,"USB EP FIFOCON","ON","OFF");

	if(!(x&(1<<3))) {
		ep[UENUM].fifoi=0;
		if(usbpid!=IN) qemu_ack();
	} else if(!(x&(1<<0))) {

		int i;
		printf("TX %u:",ep[UENUM].fifoi);
		for(i=0;i<ep[UENUM].fifoi;i++) { printf(" %02x",ep[UENUM].fifo[i]); }
		printf("\n");

		sim_gfs_write();
		ep[UENUM].fifoi=0;
	}
}

static uint8_t UHWCON;

static void avr_IOWd7(uint8_t a,uint8_t x) {
	if((x&1)!=(UHWCON&1)) { printf("  USB Pad Regulator: %s\n",x&1?"ENABLED":"DISABLED"); }
	UHWCON=x;
}

static uint8_t USBCON;

static void avr_IOWd8(uint8_t a,uint8_t x) {
	uint8_t o=USBCON;
	rb(7,x,o,"USB","ON","OFF");
	rb(5,x,o,"USB CLOCK","FREEZED","ENABLED");
	rb(4,x,o,"USB VBUS PAD","ON","OFF");
	rb(0,x,o,"USB VBUS INT","ON","OFF");

	if(!(x&(1<<5))) {
		schedule(250,sim_usb_initspeed);
	}
	USBCON=x;
}

static uint8_t UDCON;

static void avr_IOWe0(uint8_t a,uint8_t x) {
	uint8_t o=UDCON;
	rb(3,x,o,"USB Reset CPU","ON","OFF");
	rb(2,x,o,"USB Low Speed","ON","OFF");
	rb(1,x,o,"USB Remote Wake-up","SEND","WTF");
	rb(0,x,o,"USB Detach","ON","OFF");
	UDCON=x;
}

static uint8_t UDIEN;

static void avr_IOWe2(uint8_t a,uint8_t x) {
        uint8_t o=UDIEN;
	rb(6,x,o,"USB UPRSMI","ON","OFF");
	rb(5,x,o,"USB EORSMI","ON","OFF");
	rb(4,x,o,"USB WAKEUPI","ON","OFF");
	rb(3,x,o,"USB EORSTI","ON","OFF");
	rb(2,x,o,"USB SOFI","ON","OFF");
	rb(0,x,o,"USB SUSPI","ON","OFF");
        UDIEN=x;
}

static void avr_IOWe3(uint8_t a, uint8_t x) {
	UDADDR=x;
}

int usb_poll(uint64_t d) {
	sim_operate();
	return 0;
}

void usb_init() {
	ADD_IOW(d7);
	ADD_IOW(d8);

	ADD_IOW(e0);
	ADD_IORW(e1);
	ADD_IOW(e2);
	ADD_IOW(e3);
	ADD_IORW(e8);
	ADD_IOW(e9);
	ADD_IOW(ea);
	ADD_IOW(eb);
	ADD_IOW(ec);
	ADD_IOW(ed);

	ADD_IOW(f0);
	ADD_IORW(f1);

	qemu_init();
}

