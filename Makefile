NPKVER:=7.9
BBVER:=1.36.1
SYSDIR_SUFFIX:=system-v3
NPKURL=https://download.mikrotik.com/routeros/$(NPKVER)/routeros-$(NPKVER)-mipsbe.npk
NPKDIR=npk
NPK=$(NPKDIR)/routeros-$(NPKVER)-mipsbe.npk
BBURL=https://busybox.net/downloads/busybox-$(BBVER).tar.bz2
BBDIR=busybox-$(BBVER)
BBMKF=$(BBDIR)/Makefile
BB=$(BBDIR)/busybox
WGET=wget
WGETFLAGS=
KEDIR=kernel-extractor
KE=$(KEDIR)/kernel-extractor
KERNELDIR=kernel
CPIODIR=$(KERNELDIR)/cpio
KERNELBIN=$(KERNELDIR)/kernel.bin
KERNELELF=$(KERNELDIR)/kernel.elf
KERNELNEWELF=$(KERNELDIR)/kernel-new.elf
UNPACK_KERNEL_SH=./unpack-kernel.sh
PACK_KERNEL_SH=./pack-kernel.sh
INITDIR=init
INIT=$(INITDIR)/init
SED=sed
SEDFLAGS=-ri
WC=wc
WCFLAGS=-c
MD=mkdir
MDFLAGS=-p
CP=cp
CPFLAGS=
RM=rm
RMFLAGS=-rf

all: unpack init pack dist
	@echo ''
	@echo '*****************************************************'
	@echo 'Copy contents of "dist" directory'
	@echo 'at root of your flash pen drive.'
	@echo 'Put "kernel/kernel-new.elf on your TFTP boot service'
	@echo 'and setup MikroTik device to boot it from.'
	@echo '*****************************************************'

dist: $(BB)
	$(RM) $(RMFLAGS) $@
	$(MD) $(MDFLAGS) $@
	$(CP) $(CPFLAGS) -R pre$@ $@/$(SYSDIR_SUFFIX)
	sed -ri 's/\{SYSDIR_SUFFIX\}/$(SYSDIR_SUFFIX)/g' $@/$(SYSDIR_SUFFIX)/etc/passwd
	sed -ri 's/\{SYSDIR_SUFFIX\}/$(SYSDIR_SUFFIX)/g' $@/$(SYSDIR_SUFFIX)/etc/svc.d/telnetd/check
	sed -ri 's/\{SYSDIR_SUFFIX\}/$(SYSDIR_SUFFIX)/g' $@/$(SYSDIR_SUFFIX)/etc/svc.d/telnetd/run
	$(MD) $(MDFLAGS) $@/$(SYSDIR_SUFFIX)/bin
	$(CP) $(CPFLAGS) $< $@/$(SYSDIR_SUFFIX)/bin/

unpack: $(KERNELELF)

init: $(INIT)
	$(CP) $(CFLAGS) $< $(CPIODIR)/

pack: $(KERNELNEWELF)

busybox: $(BB)

$(NPK):
	$(MD) $(MDFLAGS) $(NPKDIR)
	$(WGET) $(WGETFLAGS) -O $@ $(NPKURL)

$(BBMKF):
	$(WGET) $(WGETFLAGS) $(BBURL) -O- | tar -xvjp

$(BB): $(BBMKF)
	$(MAKE) -C $(BBDIR) defconfig
	$(SED) $(SEDFLAGS) 's/^(#\s+?)?(CONFIG_STATIC)(=|\s|$$).+?$$/\2=y/g' $(BBDIR)/.config
	$(SED) $(SEDFLAGS) 's/^(#\s+?)?(CONFIG_DESKTOP)(=|\s|$$).+?$$/\2=n/g' $(BBDIR)/.config
	$(SED) $(SEDFLAGS) 's/^(#\s+?)?(CONFIG_PIE)(=|\s|$$).+?$$/\2=n/g' $(BBDIR)/.config
	$(SED) $(SEDFLAGS) 's/^(#\s+?)?(CONFIG_EXTRA_CFLAGS)(=|\s|$$).+?$$/\2="-march=mips32r2"/g' $(BBDIR)/.config
	$(SED) $(SEDFLAGS) 's/^(#\s+?)?(CONFIG_EXTRA_LDFLAGS)(=|\s|$$).+?$$/\2="-march=mips32r2 -no-pie"/g' $(BBDIR)/.config
	$(SED) $(SEDFLAGS) 's|^(#define\s+?_PATH_[^\s]+?\s+?")(/etc/.+?")$$|\1/ram/disks/usb1-part1/$(SYSDIR_SUFFIX)\2|g' $(BBDIR)/include/libbb.h
	$(MAKE) -j $$(nproc) -C $(BBDIR) SUBARCH=mips32r2 busybox
	$(WC) $(WCFLAGS) $@

$(KE):
	$(MAKE) -C $(KEDIR)

$(INIT):
	$(MAKE) -C $(INITDIR) SYSDIR_SUFFIX=$(SYSDIR_SUFFIX)

$(KERNELBIN): $(NPK) $(KE)
	$(MD) $(MDFLAGS) $(KERNELDIR)
	$(KE) $< $@

$(KERNELELF): $(KERNELBIN) $(UNPACK_KERNEL_SH)
	$(UNPACK_KERNEL_SH) $<
	$(WC) $(WCFLAGS) $@

$(KERNELNEWELF): $(INIT) $(PACK_KERNEL_SH) $(KERNELELF)
	$(PACK_KERNEL_SH)
	$(WC) $(WCFLAGS) $@

clean:
	$(RM) $(RMFLAGS) $(KERNELDIR) dist
	$(MAKE) -C $(KEDIR) clean
	$(MAKE) -C $(INITDIR) clean

distclean: clean
	$(RM) $(RMFLAGS) $(NPKDIR) $(BBDIR)

.PHONY: all dist unpack init pack busybox clean distclean
