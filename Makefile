obj-m:=netmirror.o

KDIR:=/lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod.c .*.cmd .depend .*.o.d .tmp_versions Module.markers netmirror.ko.unsigned Module.symvers modules.order
