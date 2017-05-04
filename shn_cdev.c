#include<linux/init.h>
#include<linux/module.h>

static int shn_init(void)
{
	printk("Hello enter\n");
	return 0;
}

static void shn_exit(void)
{
	printk("Hello exit\n");
}

module_init(shn_init);
module_exit(shn_exit);

MODULE_AUTHOR("Marvin");
MODULE_LICENSE("GPL");
