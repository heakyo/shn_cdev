#ifndef __SHN_CDEV_H__
#define __SHN_CDEV_H__

#include<linux/init.h>
#include<linux/module.h>
#include<linux/pci.h>
#include<linux/cdev.h>
#include<linux/interrupt.h>
#include<linux/fs.h>

/* define */
#define PCI_VENDOR_ID_SHANNON 	0x1CB0
#define PCI_DEVICE_ID_SHANNON_25A5 0x25A5
#define PCI_DEVICE_ID_SHANNON_05A5 0x05A5

#define SHNDEV_NAME "shn_cdev"

#define MAX_LUNS 512
#define MAX_LUNS_NLONG ((MAX_LUNS + 8 * sizeof(unsigned long) - 1) / (8 * sizeof(unsigned long)))
#define MAX_LUNS_NBYTE (MAX_LUNS_NLONG * sizeof(unsigned long))

// BAR selection
#define BAR0 0x01
#define BAR1 0x02

/* struct */
struct shn_cdev {
	struct cdev cdev;
	struct device *device;
	struct class *class;
	struct pci_dev *pdev;
	dev_t devno;
	char name[32];
	int bar_mark;

	resource_size_t bar_host_phymem_addr;
	resource_size_t bar_host_phymem_len;

	unsigned int __iomem *mmio;

	struct tasklet_struct tasklet;

	unsigned long phylun_bitmap[MAX_LUNS_NLONG];
	unsigned long done_phylun_bitmap[MAX_LUNS_NLONG];
};

void shn_do_tasklet(unsigned long data);

#endif

