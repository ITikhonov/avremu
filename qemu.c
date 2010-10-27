
#define _GNU_SOURCE

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "qemu.h"

static struct pollfd ep0;

void qemu_init() {
        struct sockaddr_un local;
        int s = socket(AF_UNIX, SOCK_DGRAM, 0);
        local.sun_family = AF_UNIX;

        strcpy(local.sun_path, "/tmp/usb");
        local.sun_path[0]=0;
        bind(s, (struct sockaddr *)&local, strlen(local.sun_path) + sizeof(local.sun_family));

        local.sun_path[0]='/';
        connect(s, (struct sockaddr *)&local, strlen(local.sun_path) + sizeof(local.sun_family));

	ep0.fd=s;
	ep0.events = POLLIN;

	qemu_detach();
	printf("QEMU link initialized\n");
}

void qemu_attach() {
        uint8_t attach=0xff;
        if(send(ep0.fd,&attach,1,0)!=1) {
		perror("QEMU device attach failed\n");
		abort();
	}
}


void qemu_detach() {
        uint8_t detach=0xfe;
        if(send(ep0.fd,&detach,1,0)!=1) {
		perror("QEMU device detach failed\n");
		abort();
	}
}


void qemu_ack() {
        uint8_t ack=0x0;
        if(send(ep0.fd,&ack,1,0)!=1) {
		perror("QEMU device ack failed\n");
		abort();
	}
}


void qemu_setaddr(uint8_t addr) {
	uint8_t pkt[2]={0xfd,addr};
        if(send(ep0.fd,&pkt,2,0)!=1) {
		perror("QEMU device ack failed\n");
		abort();
	}
}

void qemu_write(uint8_t *buf, int len) {
    uint8_t pre=0;
    struct iovec io[2]={{&pre,1},{buf,len}};
    struct msghdr h={0,0,io,2,0,0,0};

    printf("QEMU sending %u bytes\n",len);

    if(sendmsg(ep0.fd,&h,0)!=1+len) {
	printf("QEMU send failed\n");
	abort();
    }
}

int qemu_poll(uint64_t ns, uint8_t *r) {
	struct timespec t={.tv_sec=0,.tv_nsec=ns};
	int ret=ppoll(&ep0,1,&t,0);
	if(ret<0) { perror("qemu ppoll"); }
	if(ret) {
		char buf[1024];
		int n=recv(ep0.fd,buf,1024,0);
		if(n>0) {
    			printf("QEMU recv %u bytes tag %u\n",n,buf[0]);
			switch(buf[0]) {
			case 0:
				memcpy(r,buf+1,8);
				return 1; //SETUP
			case 1:
				return 2|(buf[1]<<4); //IN
			case 2:
				memcpy(r,buf+2,n-2);
				return 3|(buf[1]<<4); //OUT
			default:
				printf("unknown tag %hhu\n",buf[0]);
				abort();
			}
		} else {
			perror("qemu read error");
			abort();
		}
	}
	return 0;
}

