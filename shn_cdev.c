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
	printk("shn cdev devno: %x\n", cdev->devno);

	cdev_init(&cdev->cdev, &shn_cdev_fops);
	cdev->cdev.owner = THIS_MODULE;

	rc = cdev_add(&cdev->cdev, cdev->devno, 1);
	if (rc) {
		printk("add cdev to system  error\n");
		goto fail_cdev_add;
	}

	cdev->class = class_create(THIS_MODULE, cdev->name);
	if (IS_ERR(cdev->class)) {
		rc = PTR_ERR(cdev->class);
		printk("create class error\n");
		goto fail_class_create;
	}

	cdev->device = device_create(cdev->class, NULL, cdev->devno, NULL, cdev->name);
	if (IS_ERR(cdev->device)) {
		rc = PTR_ERR(cdev->device);
		printk("create device error\n");
		goto fail_dev_create;
	}

	return 0;

fail_dev_create:
	class_destroy(cdev->class);
fail_class_create:
	cdev_del(&cdev->cdev);
fail_cdev_add:
	unregister_chrdev_region(cdev->devno, 1);
out:
	return rc;
}

static void unregister_shn_cdev(struct shn_cdev *cdev)
{
	device_destroy(cdev->class, cdev->devno);
	class_destroy(cdev->class);
	cdev_del(&cdev->cdev);
	unregister_chrdev_region(cdev->devno, 1);
}

/* 0: little endian  1: big endian */
static int check_endian(void)
{
#define LITTLE_ENDIAN 	0
#define BIG_ENDIAN 	1

	int x = 1;
	char *p = (char *)&x;

	return (*p ? LITTLE_ENDIAN : BIG_ENDIAN);
}

static int shn_cdev_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int rc = 0;

	struct shn_cdev *cdev;

	printk("%s PAGE_SIZE=%ld dma_addr_t=%ld\n",
		(check_endian() ? "Big-Endian" : "Little-Endian"), PAGE_SIZE, sizeof(dma_addr_t));

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
		goto fail_en_pci_dev;

	/* use DMA */
	pci_set_master(dev);

	rc = pci_request_selected_regions(dev, cdev->bar_mark, SHNDEV_NAME);
	if (rc)
		goto fail_req_regions;

	cdev->bar_host_phymem_addr = pci_resource_start(dev, 0);
	cdev->bar_host_phymem_len = pci_resource_len(dev, 0);
	cdev->mmio = ioremap(cdev->bar_host_phymem_addr, cdev->bar_host_phymem_len);
	if (!cdev->mmio) {
		printk("ioremap phyaddr %p error\n", (void *)cdev->bar_host_phymem_addr);
		goto fail_map;
	}

	rc = pci_set_dma_mask(dev, DMA_BIT_MASK(32));
	if (rc) {
		printk("Set dma mask error\n");
		goto fail_set_dma_mask;
	}

	tasklet_init(&cdev->tasklet, shn_do_tasklet, (unsigned long)cdev);
	rc = request_irq(dev->irq, shn_cdev_irq, IRQF_SHARED, cdev->name, (void *)cdev);
	if (rc) {
		printk("request irq error\n");
		goto fail_set_dma_mask;
	}
	printk("irq: %d\n", dev->irq);

	/* register shn cdev */
	rc = register_shn_cdev(cdev);
	if (rc) {
		printk("register shn cdev error\n");
		goto fail_reg_shn;
	}

	printk("probe success\n");
	return 0;

fail_reg_shn:
	free_irq(dev->irq, cdev);
fail_set_dma_mask:
	iounmap(cdev->mmio);
fail_map:
	pci_release_selected_regions(dev, cdev->bar_mark);
fail_req_regions:
	pci_disable_device(dev);
fail_en_pci_dev:
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
	return pci_register_driver(&shn_cdev_driver);
}

static void shn_exit(void)
{
	pci_unregister_driver(&shn_cdev_driver);
}

module_init(shn_init);
module_exit(shn_exit);

MODULE_AUTHOR("Marvin");
MODULE_LICENSE("GPL");
