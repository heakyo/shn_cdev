#ifndef __SHN_CDEV_H__
#define __SHN_CDEV_H__

#include<linux/init.h>
#include<linux/module.h>
#include<linux/pci.h>
#include<linux/cdev.h>
#include<linux/interrupt.h>

/* define */
#define PCI_VENDOR_ID_SHANNON 	0x1CB0
#define PCI_DEVICE_ID_SHANNON_25A5 0x25a5
#define PCI_DEVICE_ID_SHANNON_05A5 0x05a5

#define SHNDEV_NAME "shn_cdev"

/* struct */
struct shn_cdev {
	struct cdev cdev;
	char name[32];
	int bar_mask;

	resource_size_t bar_host_phymem_addr;
	resource_size_t bar_host_phymem_len;

};

#endif

