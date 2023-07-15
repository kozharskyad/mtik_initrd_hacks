#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>

typedef unsigned char BOOL;

#ifdef FALSE
#undef FALSE
#endif
#define FALSE ((unsigned char)0)

#ifdef TRUE
#undef TRUE
#endif
#define TRUE (!FALSE)

static const unsigned char KERNEL_MAGIC[] = {0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0A, 0x04};
static const unsigned char KERNEL_TAIL_SPLIT[] = {0x00, 0x78, 0x01, 0x00, 0x00, 0x80, 0xff, 0x7f};

static BOOL memcmp_real(const unsigned char *mem1, const unsigned char *mem2, size_t mem1_size, size_t mem2_size) {
	size_t i;

	if (mem1_size != mem2_size) {
		return FALSE;
	}

	for (i = 0; i < mem1_size; i++) {
		if (mem1[i] != mem2[i]) {
			return FALSE;
		}
	}

	return TRUE;
}

static unsigned char *kernel_start_search(unsigned char *start, unsigned char *end) {
	unsigned char *uint8_ptr = start;

	do {
		if (memcmp_real(uint8_ptr, KERNEL_MAGIC, sizeof(KERNEL_MAGIC), sizeof(KERNEL_MAGIC))) {
			uint8_ptr += 12;

			if (memcmp_real(uint8_ptr, KERNEL_TAIL_SPLIT, sizeof(KERNEL_TAIL_SPLIT), sizeof(KERNEL_TAIL_SPLIT))) {
				return uint8_ptr + 3;
			}
		}

		uint8_ptr += 1;
	} while (uint8_ptr + 20 <= end);

	return (unsigned char *)NULL;
}

static BOOL kernel_extract(unsigned char *start, unsigned char *end, char *out_file) {
	int fd;
	size_t len, chunk_size;
	unsigned char *uint8_ptr = start;

	fd = open(out_file, O_WRONLY | O_CREAT, 0644);

	if(fd > 0) {
		do {
			BOOL is_last_chunk = uint8_ptr[0] == 1;

			chunk_size = (uint8_ptr[2] << 8) | uint8_ptr[1];
			uint8_ptr += 5;

			if (chunk_size > (size_t)(end - uint8_ptr)) {
				return FALSE;
			}

			len = write(fd, uint8_ptr, chunk_size);

			if (len != chunk_size) {
				return FALSE;
			}

			uint8_ptr += chunk_size;

			if (is_last_chunk) {
				break;
			}
		} while (uint8_ptr + 5 <= end);
	
		close(fd);

		return TRUE;
	} else {
		fprintf(stderr, "Error open '%s': %s\n", out_file, strerror(errno));
	}

	return FALSE;
}

int main(int argc, char *argv[]) {
	int ret = EXIT_FAILURE, fd = -1;
	struct stat st;
	char *in_file;
	char *out_file;
	unsigned char *data = (unsigned char *)MAP_FAILED;

	if (argc < 3) {
		printf("Usage:\n  %s <npk_file_path> <kernel_out_file_path>\n", argv[0]);

		return ret;
	}

	in_file = argv[1];
	out_file = argv[2];

	fd = open(in_file, O_RDONLY);

	if (fd > -1) {
		if (fstat(fd, &st) == 0) {
			data = (unsigned char *)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, (off_t)0);

			if ((void *)data != MAP_FAILED) {
				unsigned char *end = data + st.st_size;
				unsigned char *start =
					kernel_start_search(data, end);

				if (start != (unsigned char *)NULL) {
					if (kernel_extract(start, end, out_file)) {
						ret = EXIT_SUCCESS;
					} else {
						fprintf(stderr, "Error extracting kernel from '%s'\n", in_file);
					}
				} else {
					fprintf(stderr, "Error searching start address of kernel in '%s'\n", in_file);
				}
			} else {
				fprintf(stderr, "Error mapping '%s': %s\n", in_file, strerror(errno));
			}
		} else {
			fprintf(stderr, "Error stat '%s': %s\n", in_file, strerror(errno));
		}
	} else {
		fprintf(stderr, "Error opening '%s': %s\n", in_file, strerror(errno));
	}

	if (data != (unsigned char *)MAP_FAILED) {
		if (munmap((void *)data, st.st_size) != 0) {
			fprintf(stderr, "Error unmapping '%s': %s\n", in_file, strerror(errno));
		}
	}

	if (fd > -1) {
		if (close(fd) != 0) {
			fprintf(stderr, "Error close '%s': %s\n", in_file, strerror(errno));
		}
	}

	return ret;
}
