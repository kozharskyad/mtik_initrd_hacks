#!/bin/sh

cd kernel/cpio
find . 2>/dev/null | cpio --quiet -o -H newc | xz --check=crc32 --lzma2=dict=512KiB > ../kernel.p3-new.xz
cd ..
cat kernel.p2.xz kernel.p3-new.xz > kernel-new.combo
${CROSS_COMPILE}objcopy --update-section initrd=kernel-new.combo kernel.elf kernel-new.elf
cd ..
