# mtik_initrd_hacks

## Basic usage

```
docker pull alpine:latest
docker run -itv$HOME/Develop/mtik_initrd_hacks:/usr/src/mtik_initrd_hacks --rm alpine:latest
apk add --no-cache --repository=http://dl-cdn.alpinelinux.org/alpine/edge/testing/ binwalk
apk add --no-cache make gcc musl-dev file xz ncurses-dev automake autoconf
wget 'https://musl.cc/mips-linux-muslsf-cross.tgz' -O-|tar -xvzpC/usr/lib/gcc/
cd /usr/src/mtik_initrd_hacks
export CROSS_COMPILE='/usr/lib/gcc/mips-linux-muslsf-cross/bin/mips-linux-muslsf-'
make NPKVER=7.9 BBVER=1.36.1
```
