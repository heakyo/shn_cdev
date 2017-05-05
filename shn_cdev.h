#ifndef __SHN_CDEV_H__
#define __SHN_CDEV_H__

#include<linux/init.h>
#include<linux/module.h>
#include<linux/pci.h>
#include<linux/cdev.h>

/* define */
#define PCI_VENDOR_ID_SHANNON 	0x1CB0
#define PCI_DEVICE_ID_SHANNON_25A5 0x25a5

/* struct */
struct shn_cdev {
	struct cdev cdev;
};

#endif

