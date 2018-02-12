#include"shn_cdev.h"

static int shn_open(struct inode *inodep, struct file *filp)
{
	struct shn_cdev *cdev;

	cdev = container_of(inodep->i_cdev, struct shn_cdev, cdev);
	filp->private_data = cdev;

	return 0;
}

static ssize_t shn_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret = count;

	return ret;
}

static ssize_t shn_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	int ret = count;

	const char s[10] = "hello\n";

	if (*ppos >= 6)
		return 0;


	if (copy_to_user(buf, s, 6)) {
		ret = -EFAULT;
	} else {
		*ppos += 6;
		ret = count;

		//printk("read %ld bytes\n", count);
	}

	return ret;
}

static int shn_ioctl(struct inode *inodep, struct file *filep, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations shn_cdev_fops = {
	.owner = THIS_MODULE,
	.open = shn_open,
	.write = shn_write,
	.read = shn_read,
	.ioctl = shn_ioctl,
};

void shn_do_tasklet(unsigned long data)
{
	printk("%s\n", __func__);
}

static irqreturn_t shn_cdev_irq(int irq, void *id)
{
	//struct shn_cdev *cdev = (struct shn_cdev *)id;

	//tasklet_schedule(&cdev->tasklet);

	return IRQ_HANDLED;
}

int register_shn_cdev(struct shn_cdev *cdev)
{
	int rc = 0;

	rc = alloc_chrdev_region(&cdev->devno, 0, 1, cdev->name);
	if (rc) {
		printk("get dev number error\n");
		goto out;
	}
	printk("shn cdev id: %x\n", MKDEV(MAJOR(cdev->devno), 0));

	cdev_init(&cdev->cdev, &shn_cdev_fops);
	cdev->cdev.owner = THIS_MODULE;

	rc = cdev_add(&cdev->cdev, MKDEV(MAJOR(cdev->devno), 0), 1);
	if (rc) {
		printk("add cdev to system  error\n");
		goto unregister_cdev_out;
	}

	return 0;

unregister_cdev_out:
	unregister_chrdev_region(cdev->devno, 1);
out:
	return rc;
}

static void unregister_shn_cdev(struct shn_cdev *cdev)
{
	unregister_chrdev_region(cdev->devno, 1);
}

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

	sprintf(cdev->name, "%s", SHNDEV_NAME);
	cdev->bar_mark = 1; // only BAR 0
	pci_set_drvdata(dev, cdev);

	rc = pci_enable_device(dev);
	if (rc)
		goto free_out;

	/* use DMA */
	pci_set_master(dev);

	rc = pci_request_selected_regions(dev, cdev->bar_mark, SHNDEV_NAME);
	if (rc)
		goto disable_dev_out;

	cdev->bar_host_phymem_addr = pci_resource_start(dev, 0);
	cdev->bar_host_phymem_len = pci_resource_len(dev, 0);
	cdev->mmio = ioremap(cdev->bar_host_phymem_addr, cdev->bar_host_phymem_len);
	if (!cdev->mmio) {
		printk("ioremap phyaddr %p error\n", (void *)cdev->bar_host_phymem_addr);
		goto release_pci_regions_out;
	}

	rc = pci_set_dma_mask(dev, DMA_BIT_MASK(32));
	if (rc) {
		printk("Set dma mask error\n");
		goto iounmap_out;
	}

	tasklet_init(&cdev->tasklet, shn_do_tasklet, (unsigned long)cdev);
	rc = request_irq(dev->irq, shn_cdev_irq, IRQF_SHARED, cdev->name, (void *)cdev);
	if (rc) {
		printk("request irq error\n");
		goto iounmap_out;
	}
	printk("irq: %d\n", dev->irq);

	/* register shn cdev */
	rc = register_shn_cdev(cdev);
	if (rc) {
		printk("register shn cdev error\n");
		goto free_irq_out;
	}

	printk("probe success\n");
	return 0;

free_irq_out:
	free_irq(dev->irq, cdev);
iounmap_out:
	iounmap(cdev->mmio);
release_pci_regions_out:
	pci_release_selected_regions(dev, cdev->bar_mark);
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

	unregister_shn_cdev(cdev);
	free_irq(dev->irq, cdev);
	iounmap(cdev->mmio);
	pci_release_selected_regions(dev, cdev->bar_mark);
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
	.name = SHNDEV_NAME,
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
