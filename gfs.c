
#define _GNU_SOURCE

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <sys/uio.h>

#include <linux/types.h>
#include <linux/usb/gadgetfs.h>
#include <linux/usb/ch9.h>

static struct pollfd ep0;
struct usb_ctrlrequest cr;

void gfs_init(uint8_t *devdesc, uint8_t *confdesc, unsigned int conflen) {
	ep0.fd=open("/dev/gadget/dummy_udc",O_RDWR);
	ep0.events = POLLIN;
	uint8_t buf[65535]={0,0,0,0};

	memcpy(buf+4,confdesc,conflen);
	memcpy(buf+4+conflen,confdesc,conflen);
	memcpy(buf+4+conflen*2,devdesc,18);
	int len=4+conflen*2+18;

	int i;
	printf("  GFS INIT %u:",len);
	for(i=0;i<len;i++) { printf(" %02x",buf[i]); }
	printf("\n");


	if(write(ep0.fd,buf,len)!=len) {
		perror("GadgetFS initialization failed");
		printf("May be\nsudo mkdir /dev/gadget\n");
		printf("id -u | sudo tee /sys/module/gadgetfs/parameters/default_uid\n");
		printf("sudo mount -t gadgetfs gadgetfs /dev/gadget\n");
		abort();
	}

	printf("GadgetFS initialized\n");
}

void gfs_write(uint8_t *d, unsigned int len) {
	if(len==0) {
		int ret=read(ep0.fd,&ret,0);
		if(ret) {
			perror("GadgetFS zero write failed");
			abort();
		}
		return;
	}

	if(write(ep0.fd,d,len)!=len) {
		perror("GadgetFS write failed");
		abort();
	}
}

int gfs_poll(uint64_t ns, uint8_t *r) {
	struct timespec t={.tv_sec=0,.tv_nsec=ns};
	int ret=ppoll(&ep0,1,&t,0);
	if(ret<0) { perror("gfs ppoll"); }
	if(ret) {
		struct usb_gadgetfs_event e;
		int ret=read(ep0.fd,&e,sizeof e);
		if(ret<=0) return 0;
		if(e.type==GADGETFS_SETUP) {
			memcpy(r,&e.u.setup,sizeof(e.u.setup));
			return 1;
		}
	}
	return 0;
}

