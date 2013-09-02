ifneq ($(KERNELRELEASE),)
ccflags-y += -O -g
obj-m:= sch_myred.o
#浮点运算
EXTRA_CFLAGS:=-mhard-float
else
KERNELBUILD:=/lib/modules/$(shell uname -r)/build

default:

	@echo "BUILD kmod"


	@make -C $(KERNELBUILD) M=$(shell pwd) modules

#make -C /lib/modules/2.6.34.1/build/ M=/home/sch_myred/ modules     这句话是注释，与上面的语句相同
endif
clean:

	@echo "CLEAN kmod"

	@rm -rf *.o

	@rm -rf .depend .*.cmd *.mod.c .tmp_versions *.symvers .*.d *.markers *.order
