obj-m:= seqfile_queuedata_myred.o
KERNELBUILD:= /lib/modules/$(shell uname -r)/build 
KBUILD_EXTRA_SYMBOLS:= /root/AQM/myred/modules/Module.symvers
default:
	@echo "BUILD kmod"

	@make	-C $(KERNELBUILD) M=$(shell pwd) modules
clean:
	@echo "CLEAN kmod"
	@rm -rf *.o
	@rm -rf .depend .*.cmd *.mod.c .tmp_versions *.symvers .*.d *.markers* *.ko
