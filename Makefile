KVER=$(shell uname -r)

kernel_modules:
	make -C /lib/modules/$(KVER)/build M=$(CURDIR) modules
clean:
	make -C /lib/modules/$(KVER)/build M=$(CURDIR) clean

