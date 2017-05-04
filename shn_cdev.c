#include"shn_cdev.h"

static const struct pci_device_id shn_cdev_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_SHANNON, PCI_DEVICE_ID_SHANNON_25A5) },
	{0, }
};

static struct pci_driver shn_cdev_driver = {
	.name = "shn_cdev",
	.id_table = shn_cdev_ids,
};

static int shn_init(void)
{
	printk("shannon cdev enter\n");
	return pci_register_driver(&shn_cdev_driver);
}

static void shn_exit(void)
{
	printk("shannon cdev exit\n");
	pci_unregister_driver(&shn_cdev_driver);
}

module_init(shn_init);
module_exit(shn_exit);

MODULE_AUTHOR("Marvin");
MODULE_LICENSE("GPL");
