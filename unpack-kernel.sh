#!/bin/sh
#
#(C) Sergey Sergeev aka adron, 2019
#

KERNEL_BIN_PATH="$1"
KERNEL_BIN_DIR="$(dirname """$KERNEL_BIN_PATH""")"
KERNEL_BIN_BINWALK_CACHE=""
XZ_OFFSETS_CACHE=""

kernel_bin_binwalk() {
  if [[ -z "$KERNEL_BIN_BINWALK_CACHE" ]]; then
    KERNEL_BIN_BINWALK_CACHE="$(binwalk """$KERNEL_BIN_PATH""")"
  fi

  echo "${KERNEL_BIN_BINWALK_CACHE}"
}

kernel_bin_binwalk_rawelf() {
  binwalk -R '\177ELF' "$KERNEL_BIN_PATH"
}

get_xz_offsets() {
  if [[ -z "$XZ_OFFSETS_CACHE" ]]; then
    XZ_OFFSETS_CACHE="$(kernel_bin_binwalk | sed -rn 's/^(\d+?)\s+?.+?\s+?xz compressed data$/\1/p')"
  fi

  echo "${XZ_OFFSETS_CACHE}"
}

get_file_size() {
  local FILE_PATH="$1"

  if [[ -f "$FILE_PATH" ]]; then
    wc -c "$FILE_PATH" | sed -rn 's/^(\d+?)\s+?.+?$/\1/p' | tr -d '\r\n'
  else
    echo -n '0'
  fi
}

unpack_kernel_bin() {
  local prev_offset
  local size
  local offset
  local p_count=0
  local xz_offsets="$(get_xz_offsets)"

  for offset in ${xz_offsets} 0; do
    if [[ -n "$prev_offset" ]]; then
      p_count=$((p_count+1))
      size=$((offset-prev_offset))

      if [[ $offset -eq 0 ]]; then
        size=104857600
      fi

      dd if="$KERNEL_BIN_PATH" of="$KERNEL_BIN_DIR/kernel.p$p_count.xz" bs=$size skip=$prev_offset count=1 iflag=skip_bytes
    fi

    prev_offset=$offset
  done
}

extract_kernel_elf() {
  local p
  local size
  local offset
  local elf_last_offset
  local first_xz_offset
  local offsets="$(kernel_bin_binwalk | sed -n 's/ELF,//p' | sed -rn 's/^(\d+?)\s+?.+?$/\1/p')"
  local kernel_elf_size=0
  local garbage_size=70

  if [[ -z "$offsets" ]]; then
    offsets="$(kernel_bin_binwalk_rawelf | sed -rn 's/^(\d+?)\s+?.+?\s+?Raw signature\s+?.+?$/\1/p')"

    if [[ -z "$offsets" ]]; then
      echo 'ELF offsets not found' >&2
      exit 1
    fi
  fi

  for offset in ${offsets}; do
    elf_last_offset=$offset
  done

  if [[ -z "$elf_last_offset" ]]; then
    echo 'Cannot get last ELF offset' >&2
    exit 1
  fi

  offsets="$(get_xz_offsets)"

  if [[ -z "$offsets" ]]; then
    echo 'Cannot get first XZ offset' >&2
    exit 1
  fi

  for offset in ${offsets}; do
    first_xz_offset=${offset}
    break
  done

  kernel_elf_size=$((first_xz_offset-elf_last_offset))

  for p in $(seq 1 3); do
    size=$(get_file_size "$KERNEL_BIN_DIR/kernel.p${p}.xz")
    kernel_elf_size=$((kernel_elf_size+size))
  done

  dd if="$KERNEL_BIN_PATH" of="$KERNEL_BIN_DIR/kernel.elf" bs=$kernel_elf_size skip=$elf_last_offset count=1 iflag=skip_bytes

  if [[ -f "$KERNEL_BIN_DIR/p3-garbage.bin" ]]; then
    size=$(get_file_size "$KERNEL_BIN_DIR/p3-garbage.bin")

    if [[ $size -gt $garbage_size ]]; then
      offset=$((size-garbage_size))
      dd if="$KERNEL_BIN_DIR/p3-garbage.bin" bs=$offset count=1 >> "$KERNEL_BIN_DIR/kernel.elf"
    fi
  fi
}

truncate_kernel_p3() {
  local garbage_size=$(get_file_size "$KERNEL_BIN_DIR/p3-garbage.bin")
  local p3_size=$(get_file_size "$KERNEL_BIN_DIR/kernel.p3.xz")

  dd if="$KERNEL_BIN_DIR/kernel.p3.xz" of="$KERNEL_BIN_DIR/kernel.p3-stripped.xz" bs=$((p3_size-garbage_size)) count=1
  cat "$KERNEL_BIN_DIR/kernel.p3-stripped.xz" > "$KERNEL_BIN_DIR/kernel.p3.xz"
  rm "$KERNEL_BIN_DIR/kernel.p3-stripped.xz"
}

extract_kernel_cpiofs() {
  local cpio_fs="$KERNEL_BIN_DIR/cpio"

  rm -rf "$cpio_fs"
  mkdir "$cpio_fs"
  cd "$cpio_fs"
  xzcat "../kernel.p3.xz" | cpio -idv
  cp init oldinit
  cd ..
}

unpack_kernel_bin
( xz -dc --single-stream > "$KERNEL_BIN_DIR/initramfs.cpio" && cat > "$KERNEL_BIN_DIR/p3-garbage.bin" ) < "$KERNEL_BIN_DIR/kernel.p3.xz"

if [[ -f "$KERNEL_BIN_DIR/p3-garbage.bin" ]]; then
  truncate_kernel_p3
fi

extract_kernel_elf
extract_kernel_cpiofs
