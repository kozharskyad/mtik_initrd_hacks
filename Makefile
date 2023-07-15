NPKVER:=$(NPKVER)
NPKURL=https://download.mikrotik.com/routeros/$(NPKVER)/routeros-$(NPKVER)-mipsbe.npk
NPKDIR=npk
NPK=$(NPKDIR)/routeros-$(NPKVER)-mipsbe.npk
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
WC=wc
WCFLAGS=-c
MD=mkdir
MDFLAGS=-p
CP=cp
CPFLAGS=
RM=rm
RMFLAGS=-rf

all: unpack init

unpack: $(KERNELELF)

init: $(INIT)
	$(CP) $(CFLAGS) $< $(CPIODIR)

pack: $(KERNELNEWELF)

$(NPKDIR):
	$(MD) $(MDFLAGS) $@

$(NPK): $(NPKDIR)
	$(WGET) $(WGETFLAGS) -O $@ $(NPKURL)

$(KE):
	$(MAKE) -C $(KEDIR)

$(INIT):
	$(MAKE) -C $(INITDIR)

$(KERNELBIN): $(NPK) $(KE)
	$(MD) $(MDFLAGS) $(KERNELDIR)
	$(KE) $< $@

$(KERNELELF): $(KERNELBIN) $(UNPACK_KERNEL_SH)
	$(UNPACK_KERNEL_SH) $<
	$(WC) $(WCFLAGS) $@

$(KERNELNEWELF): $(PACK_KERNEL_SH) $(KERNELELF)
	$(PACK_KERNEL_SH)
	$(WC) $(WCFLAGS) $@

clean:
	$(RM) $(RMFLAGS) $(KERNELDIR)
	$(MAKE) -C $(KEDIR) clean
	$(MAKE) -C $(INITDIR) clean

.PHONY: all unpack init pack clean
