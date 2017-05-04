KVER=$(shell uname -r)

obj-m += shn_cdev.o

kernel_modules:
	make -C /lib/modules/$(KVER)/build M=$(CURDIR) modules
clean:
	make -C /lib/modules/$(KVER)/build M=$(CURDIR) clean

