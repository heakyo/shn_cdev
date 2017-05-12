#include"shn_cdev.h"

static int shn_cdev_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int rc = 0;

	struct shn_cdev *cdev;
	printk("%s\n", __func__);
	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev) {
		rc = -ENOMEM;
		goto out;
	}

	cdev->bar_mask = 1;
	pci_set_drvdata(dev, cdev);

	rc = pci_enable_device(dev);
	if (rc)
		goto free_out;

	/* use DMA */
	pci_set_master(dev);

	rc = pci_request_selected_regions(dev, cdev->bar_mask, "shn_dev");
	if (rc)
		goto disable_dev_out;

	cdev->bar_host_phymem_addr = pci_resource_start(dev, 0);
	cdev->bar_host_phymem_len = pci_resource_len(dev, 0);

	rc = pci_set_dma_mask(dev,DMA_BIT_MASK(32));
	if (rc) {
		printk("Set dma mask error\n");
		goto release_pci_regions_out;
	}

	printk("probe success\n");
	return 0;

release_pci_regions_out:
	pci_release_selected_regions(dev, cdev->bar_mask);
disable_dev_out:
	pci_disable_device(dev);
free_out:
	kfree(cdev);
out:
	return rc;
}

static void shn_cdev_remove(struct pci_dev *dev)
{

	struct shn_cdev *cdev;
	printk("%s\n", __func__);

	cdev = pci_get_drvdata(dev);

	pci_release_selected_regions(dev, cdev->bar_mask);
	pci_disable_device(dev);
	kfree(cdev);

	printk("remove success\n");
}

static const struct pci_device_id shn_cdev_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_SHANNON, PCI_DEVICE_ID_SHANNON_25A5) },
	{ PCI_DEVICE(PCI_VENDOR_ID_SHANNON, PCI_DEVICE_ID_SHANNON_05A5) },
	{0, }
};
MODULE_DEVICE_TABLE(pci, shn_cdev_ids);

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
