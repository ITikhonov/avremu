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

int gfs_init();
int gfs_poll(uint64_t ns, uint8_t *r);

static uint8_t UDINT; // 0xe1
static uint8_t UENUM; // 0xe9

static enum {WAIT_INIT,REQUEST_DEV,WAIT_DEV,REQUEST_CONF,SET_ADDRESS} usb_state=WAIT_INIT;

static struct {
	uint8_t UEINTX; // e8
	uint8_t UECONX; // eb
	uint8_t UECFG0X; // ec
	uint8_t UECFG1X; // ed
	uint8_t UEIENX; // f0

	uint8_t fifo[128];
	int fifoi;
	int write;
} ep[6];

static void sim_usb_initspeed() {
	printf("!! USB INIT SPEED\n");
	UDINT=1<<3;
	intr(0x14);
}


static uint8_t pkt_request_dev[8]={0x00,0x06, 0x00,0x01, 0x00,0x00, 0x12,0x00};

static void sim_request_dev() {
	printf("!! USB REQUEST DEV\n");
	ep[0].UEINTX=1<<3;
	memcpy(ep[0].fifo,pkt_request_dev,sizeof(pkt_request_dev));
	ep[0].fifoi=0;
	intr(0x16);
}

#if 0

static uint8_t pkt_setaddr[8]={0x00,0x05, 0x12,0x34, 0x00,0x00, 0x00,0x00};

static void sim_usb_setaddr() {
	printf("!! USB SETADDR\n");
	ep[0].UEINTX=1<<3;
	memcpy(ep[0].fifo,pkt_setaddr,sizeof(pkt_setaddr));
	ep[0].fifoi=0;

	intr(0x16);
}
#endif

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
	if(UENUM==0 && x&1) { usb_state=REQUEST_DEV; }
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

	if(!(x&(1<<3))) { ep[0].fifoi=0; }
	else if(!(x&(1<<0))) {
		printf("on TXINI OFF send packet back!\n");
		abort();
	}

	ep[UENUM].UEINTX=x;
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


#if 0
int real_packet() {
	if(gfs_poll(d,ep[0].fifo)) {
		printf("## GFS got setup\n");
		ep[0].UEINTX|=1<<3;
		ep[0].fifoi=0;

		intr(0x16);
		return 1;
	}
	return 0;
}
#endif

int usb_poll(uint64_t d) {
	if(getI() && ep[0].UEIENX&(1<<3)) {
		switch(usb_state) {
		case WAIT_INIT:
		case WAIT_DEV: 
			break;
		case REQUEST_DEV:
			sim_request_dev();
			usb_state=WAIT_DEV;
			break;
		case REQUEST_CONF: 
		case SET_ADDRESS: 
			abort();
		}
	}
	return 0;
}

void usb_init() {
	ADD_IOW(d7);
	ADD_IOW(d8);

	ADD_IOW(e0);
	ADD_IORW(e1);
	ADD_IOW(e2);
	ADD_IORW(e8);
	ADD_IOW(e9);
	ADD_IOW(eb);
	ADD_IOW(ec);
	ADD_IOW(ed);

	ADD_IOW(f0);
	ADD_IORW(f1);
}
