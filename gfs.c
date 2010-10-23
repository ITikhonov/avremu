
#define _GNU_SOURCE

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>

#include <linux/types.h>
#include <linux/usb/gadgetfs.h>
#include <linux/usb/ch9.h>

#define VENDOR_ID               0x16C0
#define PRODUCT_ID              0x0480



#define ENDPOINT0_SIZE		32
#define RAWHID_INTERFACE	0
#define RAWHID_TX_ENDPOINT	1
#define RAWHID_RX_ENDPOINT	2

#define RAWHID_TX_SIZE          64      // transmit packet size
#define RAWHID_TX_INTERVAL      2       // max # of ms between transmit packets
#define RAWHID_RX_SIZE          64      // receive packet size
#define RAWHID_RX_INTERVAL      8       // max # of ms between receive packets

#define LSB(n) (n & 255)
#define MSB(n) ((n >> 8) & 255)

#define RAWHID_USAGE_PAGE       0xFFAB  // recommended: 0xFF00 to 0xFFFF
#define RAWHID_USAGE            0x0200  // recommended: 0x0100 to 0xFFFF

#pragma pack(push,1)

static uint8_t rawhid_hid_report_desc[] = {
        0x06, LSB(RAWHID_USAGE_PAGE), MSB(RAWHID_USAGE_PAGE),
        0x0A, LSB(RAWHID_USAGE), MSB(RAWHID_USAGE),
        0xA1, 0x01,                             // Collection 0x01
        0x75, 0x08,                             // report size = 8 bits
        0x15, 0x00,                             // logical minimum = 0
        0x26, 0xFF, 0x00,                       // logical maximum = 255
        0x95, RAWHID_TX_SIZE,                   // report count
        0x09, 0x01,                             // usage
        0x81, 0x02,                             // Input (array)
        0x95, RAWHID_RX_SIZE,                   // report count
        0x09, 0x02,                             // usage
        0x91, 0x02,                             // Output (array)
        0xC0                                    // end collection
};



#define CONFIG1_DESC_SIZE        (9+9+9+7+7)
#define RAWHID_HID_DESC_OFFSET   (9+9)
static uint8_t config1_descriptor[CONFIG1_DESC_SIZE*2+18+4] = {
	0,0,0,0,


	// configuration descriptor, USB spec 9.6.3, page 264-266, Table 9-10
	9, 					// bLength;
	2,					// bDescriptorType;
	CONFIG1_DESC_SIZE&255,			// wTotalLength
	CONFIG1_DESC_SIZE>>8,
	1,					// bNumInterfaces
	1,					// bConfigurationValue
	0,					// iConfiguration
	0xC0,					// bmAttributes
	50,					// bMaxPower

	// interface descriptor, USB spec 9.6.5, page 267-269, Table 9-12
	9,					// bLength
	4,					// bDescriptorType
	RAWHID_INTERFACE,			// bInterfaceNumber
	0,					// bAlternateSetting
	2,					// bNumEndpoints
	0x03,					// bInterfaceClass (0x03 = HID)
	0x00,					// bInterfaceSubClass (0x01 = Boot)
	0x00,					// bInterfaceProtocol (0x01 = Keyboard)
	0,					// iInterface
	// HID interface descriptor, HID 1.11 spec, section 6.2.1
	9,					// bLength
	0x21,					// bDescriptorType
	0x11, 0x01,				// bcdHID
	0,					// bCountryCode
	1,					// bNumDescriptors
	0x22,					// bDescriptorType
	sizeof(rawhid_hid_report_desc),		// wDescriptorLength
	0,
	// endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
	7,					// bLength
	5,					// bDescriptorType
	RAWHID_TX_ENDPOINT | 0x80,		// bEndpointAddress
	0x03,					// bmAttributes (0x03=intr)
	RAWHID_TX_SIZE, 0,			// wMaxPacketSize
	RAWHID_TX_INTERVAL,			// bInterval
	// endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
	7,					// bLength
	5,					// bDescriptorType
	RAWHID_RX_ENDPOINT,			// bEndpointAddress
	0x03,					// bmAttributes (0x03=intr)
	RAWHID_RX_SIZE, 0,			// wMaxPacketSize
	RAWHID_RX_INTERVAL,			// bInterval

	// configuration descriptor, USB spec 9.6.3, page 264-266, Table 9-10
	9, 					// bLength;
	2,					// bDescriptorType;
	CONFIG1_DESC_SIZE&255,			// wTotalLength
	CONFIG1_DESC_SIZE>>8,
	1,					// bNumInterfaces
	1,					// bConfigurationValue
	0,					// iConfiguration
	0xC0,					// bmAttributes
	50,					// bMaxPower

	// interface descriptor, USB spec 9.6.5, page 267-269, Table 9-12
	9,					// bLength
	4,					// bDescriptorType
	RAWHID_INTERFACE,			// bInterfaceNumber
	0,					// bAlternateSetting
	2,					// bNumEndpoints
	0x03,					// bInterfaceClass (0x03 = HID)
	0x00,					// bInterfaceSubClass (0x01 = Boot)
	0x00,					// bInterfaceProtocol (0x01 = Keyboard)
	0,					// iInterface
	// HID interface descriptor, HID 1.11 spec, section 6.2.1
	9,					// bLength
	0x21,					// bDescriptorType
	0x11, 0x01,				// bcdHID
	0,					// bCountryCode
	1,					// bNumDescriptors
	0x22,					// bDescriptorType
	sizeof(rawhid_hid_report_desc),		// wDescriptorLength
	0,
	// endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
	7,					// bLength
	5,					// bDescriptorType
	RAWHID_TX_ENDPOINT | 0x80,		// bEndpointAddress
	0x03,					// bmAttributes (0x03=intr)
	RAWHID_TX_SIZE, 0,			// wMaxPacketSize
	RAWHID_TX_INTERVAL,			// bInterval
	// endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
	7,					// bLength
	5,					// bDescriptorType
	RAWHID_RX_ENDPOINT,			// bEndpointAddress
	0x03,					// bmAttributes (0x03=intr)
	RAWHID_RX_SIZE, 0,			// wMaxPacketSize
	RAWHID_RX_INTERVAL,			// bInterval

        18,                                     // bLength
        1,                                      // bDescriptorType
        0x00, 0x02,                             // bcdUSB
        0,                                      // bDeviceClass
        0,                                      // bDeviceSubClass
        0,                                      // bDeviceProtocol
        ENDPOINT0_SIZE,                         // bMaxPacketSize0
        LSB(VENDOR_ID), MSB(VENDOR_ID),         // idVendor
        LSB(PRODUCT_ID), MSB(PRODUCT_ID),       // idProduct
        0x00, 0x01,                             // bcdDevice
        0,                                      // iManufacturer
        0,                                      // iProduct
        0,                                      // iSerialNumber
        1                                       // bNumConfigurations


};


#pragma pack(pop)

static struct pollfd ep0;
struct usb_ctrlrequest cr;

void gfs_init() {
	ep0.fd=open("/dev/gadget/dummy_udc",O_RDWR);
	ep0.events = POLLIN;
	if(write(ep0.fd,config1_descriptor,sizeof(config1_descriptor))!=sizeof(config1_descriptor)) {
		printf("GadgetFS sorta fucked\n");
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

