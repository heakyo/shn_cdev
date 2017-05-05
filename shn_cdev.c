#include"shn_cdev.h"

static int shn_cdev_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	printk("%s\n", __func__);
	int rc;

	rc = pci_enable_device(dev);
	if (rc)
		goto out;

	pci_set_master(dev);

	printk("probe success\n");
	return 0;

out:
	return rc;
}

static void shn_cdev_remove(struct pci_dev *dev)
{
	printk("%s\n", __func__);

	pci_disable_device(dev);
	printk("remove success\n");
}

static const struct pci_device_id shn_cdev_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_SHANNON, PCI_DEVICE_ID_SHANNON_25A5) },
	{0, }
};

static struct pci_driver shn_cdev_driver = {
	.name = "shn_cdev",
	.id_table = shn_cdev_ids,
	.probe = shn_cdev_probe,
	.remove = shn_cdev_remove,
};

static int shn_init(void)
{
	printk("%s\n", __func__);
	return pci_register_driver(&shn_cdev_driver);
}

static void shn_exit(void)
{
	printk("%s\n", __func__);
	pci_unregister_driver(&shn_cdev_driver);
}

module_init(shn_init);
module_exit(shn_exit);

MODULE_AUTHOR("Marvin");
MODULE_LICENSE("GPL");
