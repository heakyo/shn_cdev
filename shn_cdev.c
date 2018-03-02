#include"shn_cdev.h"

static int shn_open(struct inode *inodep, struct file *filp)
{
	struct shn_cdev *cdev;

	cdev = container_of(inodep->i_cdev, struct shn_cdev, cdev);
	filp->private_data = cdev;

	return 0;
}

/*
 * It's better to operate BAR in DW not in memory copy
 * Because some registers will take effect one by one
 */
static ssize_t shn_write(struct file *filp, const char __user *data, size_t count, loff_t *ppos)
{
	struct shn_cdev *cdev = filp->private_data;
	int ret = count;
	int i = 0;
	unsigned int *buf = NULL;
	const int dw_cnt = count / DW_SIZE;

	if (count == 0)
		return 0;

	if ((count % DW_SIZE != 0) || (*ppos % DW_SIZE != 0)) {
		pr_err("[%s] position %lld and count %ld must be aligned with 4\n", __func__, *ppos, count);
		return -EINVAL;
	}

	if (*ppos + count > cdev->bar_host_phymem_len) {
		pr_err("[%s] position %lld and count %ld overflow device address range\n", __func__, *ppos, count);
		return -EINVAL;
	}

	buf = kmalloc(count, GFP_KERNEL);
	if (NULL == buf) {
		pr_err("[%s] alloc memory error\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(buf, data, count)) {
		ret = -EFAULT;
	} else {
		for (i = 0; i < dw_cnt; i++)
			writel(buf[i], cdev->mmio + *ppos / DW_SIZE + i);
		*ppos += count;
		ret = count;
	}

	kfree(buf);
	return ret;
}

static ssize_t shn_read(struct file *filp, char __user *data, size_t count, loff_t *ppos)
{
	struct shn_cdev *cdev = filp->private_data;
	int ret = 0;

	if (count == 0)
		return 0;

	if ((count % 4 != 0) || (*ppos % 4 != 0)) {
		pr_err("[%s] position %lld and count %ld must be aligned with 4\n", __func__, *ppos, count);
		return -EINVAL;
	}

	if (*ppos + count > cdev->bar_host_phymem_len) {
		pr_err("[%s] position %lld and count %ld overflow device address range\n", __func__, *ppos, count);
		return -EINVAL;
	}

	if (copy_to_user(data, (unsigned char *)cdev->mmio + *ppos, count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;
	}

	return ret;
}

static int shn_ioctl(struct inode *inodep, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct shn_cdev *cdev = filp->private_data;
	struct shn_ioctl_data ioctl_data;

	if (SHNCDEV_IOC_MAGIC != _IOC_TYPE(cmd) ) {
		pr_err("[%s] command type %c error\n", __func__, _IOC_TYPE(cmd));
		return -ENOTTY;
	}

	if (copy_from_user(&ioctl_data, (struct shn_ioctl_data __user *)arg, sizeof(ioctl_data)))
		return -EFAULT;

	switch (cmd) {
	case SHNCDEV_IOC_GB:
		if (ioctl_data.bar == 0) {
			ioctl_data.size = cdev->bar_host_phymem_len;
		} else if (ioctl_data.bar == 1) {
			/* TODO */
		} else {
			return -EINVAL;
		}
		break;
	case SHNCDEV_IOC_GF:
		//printk("usr_addr: %p size: %d\n", ioctl_data.usr_addr, ioctl_data.size);
		memcpy(ioctl_data.usr_addr, cdev->qmem, ioctl_data.size);
		break;
	case SHNCDEV_IOC_GD:
		memcpy(ioctl_data.usr_addr, cdev->domain_info, ioctl_data.size);
		break;
	case SHNCDEV_IOC_GS:
		memcpy(ioctl_data.usr_addr, &cdev->subsystem_id, ioctl_data.size);
		break;
	case SHNCDEV_IOC_WM:
		//printk("size: %d kernel_addr: %p user_addr: %p\n", ioctl_data.size, ioctl_data.qmem.kernel_addr, ioctl_data.usr_addr);
		memcpy(ioctl_data.qmem.kernel_addr, ioctl_data.usr_addr, ioctl_data.size);
		break;
	case SHNCDEV_IOC_RM:
		//printk("size: %d kernel_addr: %p user_addr: %p\n", ioctl_data.size, ioctl_data.qmem.kernel_addr, ioctl_data.usr_addr);
		memcpy(ioctl_data.usr_addr, ioctl_data.qmem.kernel_addr, ioctl_data.size);
		break;
	default:
		printk("Unkonwn command\n");
		return -EINVAL;

	}

	if(copy_to_user((struct shn_ioctl_data __user *)arg, &ioctl_data, sizeof(ioctl_data)))
		return -EFAULT;

	return 0;
}

static loff_t shn_llseek(struct file *filp, loff_t offset, int whence)
{
	struct shn_cdev *cdev = filp->private_data;
	loff_t ret = 0;

	switch (whence) {
	case SEEK_SET:
		ret = offset;
		break;
	case SEEK_CUR:
		ret = filp->f_pos + offset;
		break;
	case SEEK_END:
		ret =  cdev->bar_host_phymem_len + offset;
		break;
	default:
		ret = -EINVAL;
	}

	if (ret < 0 || ret >= cdev->bar_host_phymem_len)
		return -EINVAL;

	filp->f_pos = ret;

	return ret;
}

static const struct file_operations shn_cdev_fops = {
	.owner = THIS_MODULE,
	.open = shn_open,
	.write = shn_write,
	.read = shn_read,
	.ioctl = shn_ioctl,
	.llseek = shn_llseek,
};

void shn_do_tasklet(unsigned long data)
{
	//printk("%s\n", __func__);
}

static irqreturn_t shn_cdev_irq(int irq, void *id)
{
	struct shn_cdev *cdev = (struct shn_cdev *)id;

	tasklet_schedule(&cdev->tasklet);

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

static void set_shn_cdev_name(struct shn_cdev *cdev)
{
	sprintf(cdev->name, "%s", SHNDEV_NAME);
}

static int get_hw_nchannel(struct shn_cdev *cdev)
{
	return (ioread32(BAR0_REG_STATIC_SYS_INFO)>>16 & 0xFF);
}

static int get_hw_nthread(struct shn_cdev *cdev)
{
	return (ioread32(BAR0_REG_STATIC_SYS_INFO)>>24 & 0x0F);
}

static int get_hw_nlun(struct shn_cdev *cdev)
{
	return ((ioread32(BAR0_REG_STATIC_SYS_INFO)>>12 & 0x0F) + 1);
}

static int get_hw_dma_addr_bits(struct shn_cdev *cdev)
{
	return ((ioread32(BAR0_REG_GLBL_CFG)>>25 & 0x01) + 1) * 32;
}

static void set_hw_dma_addr_bits(struct shn_cdev *cdev, int dma_addr_bits)
{
	char dma_bit = dma_addr_bits / 32 - 1;

	iowrite32((ioread32(BAR0_REG_GLBL_CFG) & (~0x02000000)) | (dma_bit<<25), BAR0_REG_GLBL_CFG);
}

static void free_qmem(struct shn_cdev *cdev)
{
	int thr = 0;

	for (thr = 0; thr < cdev->hw_threads; thr++) {
		if (cdev->qmem[thr].kernel_addr == NULL)
			break;
		dma_free_coherent(&cdev->pdev->dev, QUEUE_MEM_SIZE, cdev->qmem[thr].kernel_addr, cdev->qmem[thr].dma_addr);
	}

	kfree(cdev->qmem);
}

static int alloc_qmem(struct shn_cdev *cdev)
{
	int thr = 0;
	int rc = 0;

	cdev->qmem = (struct shn_qmem *)kzalloc(sizeof(*cdev->qmem) * cdev->hw_threads, GFP_KERNEL);
	if (NULL == cdev->qmem) {
		printk("alloc queue memory failed\n");
		rc = -ENOMEM;
		goto out;
	}

	for (thr = 0; thr < cdev->hw_threads; thr++) {
		cdev->qmem[thr].kernel_addr = dma_alloc_coherent(&cdev->pdev->dev, QUEUE_MEM_SIZE, &cdev->qmem[thr].dma_addr, GFP_KERNEL);
		if (NULL == cdev->qmem[thr].kernel_addr) {
			printk("dma alloc coherent failed\n");
			rc = -ENOMEM;
			goto fail_alloc_dma_addr;
		}

		memset(cdev->qmem[thr].kernel_addr, 0x0, QUEUE_MEM_SIZE);
		//printk("kernel addr: %p dma addr: %llx\n", cdev->qmem[thr].kernel_addr, cdev->qmem[thr].dma_addr);
	}

	return 0;

fail_alloc_dma_addr:
	free_qmem(cdev);
out:
	return rc;
}

static int shn_cdev_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int rc = 0;

	struct shn_cdev *cdev;

	printk("%s PAGE SIZE: %ld DMA Addr Bits: %d\n",
		(check_endian() ? "Big-Endian" : "Little-Endian"), PAGE_SIZE, DMA_ADDR_BITS);

	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (NULL == cdev) {
		printk("alloc shn_cdev failed\n");
		rc = -ENOMEM;
		goto out;
	}

	// default all luns exist
	memset(cdev->phylun_bitmap, 0xFF, sizeof(cdev->phylun_bitmap));
	memset(cdev->done_phylun_bitmap, 0xFF, sizeof(cdev->done_phylun_bitmap));

	cdev->bar_mark = BAR0; // only BAR 0
	cdev->pdev = dev;
	pci_set_drvdata(dev, cdev); // to get shn_cdev in other functions

	set_shn_cdev_name(cdev);

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
	if (!cdev->bar_host_phymem_addr || !cdev->bar_host_phymem_len) {
		printk("hardware BAR memory failed\n");
		rc = -EIO;
		goto fail_map;
	}

	cdev->mmio = ioremap(cdev->bar_host_phymem_addr, cdev->bar_host_phymem_len);
	if (!cdev->mmio) {
		printk("ioremap phyaddr %p failed\n", (void *)cdev->bar_host_phymem_addr);
		rc = -ENOMEM;
		goto fail_map;
	}
	printk("%s BAR0 Host: phymem = 0x%llx, virmem: 0x%p, len = 0x%llx\n",
		cdev->name, cdev->bar_host_phymem_addr, cdev->mmio, cdev->bar_host_phymem_len);

	cdev->hw_nchannel = get_hw_nchannel(cdev);
	cdev->hw_nthread = get_hw_nthread(cdev);
	cdev->hw_nlun = get_hw_nlun(cdev);
	printk("hw nchannel:%d hw nthread:%d  hw nlun:%d\n", cdev->hw_nchannel, cdev->hw_nthread, cdev->hw_nlun);
	if (0x0 == cdev->hw_nchannel || 0xFF == cdev->hw_nchannel || 0x0 == cdev->hw_nthread || 0x0 == cdev->hw_nlun) {
		rc = -EIO;
		goto fail_map;
	}
	cdev->hw_threads = cdev->hw_nchannel * cdev->hw_nthread;
	cdev->hw_luns = cdev->hw_threads * cdev->hw_nlun;

	// set DMA addr
	set_hw_dma_addr_bits(cdev, DMA_ADDR_BITS);
	if (DMA_ADDR_BITS != get_hw_dma_addr_bits(cdev)) {
		printk("set hw DMA failed\n");
		rc = -EIO;
		goto fail_map;
	}

	rc = pci_set_dma_mask(dev, DMA_BIT_MASK(DMA_ADDR_BITS));
	if (rc) {
		printk("set dma mask failed\n");
		goto fail_set_dma_mask;
	}

	if (alloc_qmem(cdev))
		goto fail_set_dma_mask;

	tasklet_init(&cdev->tasklet, shn_do_tasklet, (unsigned long)cdev);
	rc = request_irq(dev->irq, shn_cdev_irq, IRQF_SHARED, cdev->name, (void *)cdev);
	if (rc) {
		printk("request irq error\n");
		goto fail_req_irq;
	}
	printk("IRQ No: %d\n", dev->irq);

	/* register shn cdev */
	rc = register_shn_cdev(cdev);
	if (rc) {
		printk("register shn cdev error\n");
		goto fail_reg_shn;
	}

	snprintf(cdev->domain_info, sizeof(cdev->domain_info), "%04x:%02x:%02x.%x",
		pci_domain_nr(dev->bus), dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
	cdev->subsystem_id = (dev->subsystem_vendor << 16) | (dev->subsystem_device);

	printk("subsystem id = %08X\n", cdev->subsystem_id);
	printk("%s success\n", __func__);
	return 0;

fail_reg_shn:
	free_irq(dev->irq, cdev);
fail_req_irq:
	free_qmem(cdev);
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
	free_qmem(cdev);
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
