#include <stdlib.h>
#include <fenv.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "init.h"

static char path_bin[PATH_MAX];
static char path_bb[PATH_MAX];
static char path_etc[PATH_MAX];
static char path_svc[PATH_MAX];
static char path_cfg[PATH_MAX];
static char srd_dir_path[] = "/system/ram/disks";
static char rd_dir_path[] = "/ram/disks";
static char *psb_system_dirs[] = {
	"/ram/disks/usb1/system",
	"/ram/disks/usb1-part1/system",
	"/ram/disks/usb1-part2/system",
	"/ram/disks/usb1-part3/system",
	"/ram/disks/usb1-part4/system",
	"/ram/disks/disk1/system",
	"/ram/disks/disk2/system",
	"/flash/rw/disk/pub/system",
	"/flash/rw/disk/flash/rw/disk/pub/system"
};
static const size_t PSB_SYSTEM_DIRS_COUNT = sizeof(psb_system_dirs) / sizeof(*psb_system_dirs);
static const char *PATH_ENV_NAME = "PATH";

static BOOL do_chmod(const char *path, int mode) {
	int fd = open(path, O_RDONLY);

	if (fd > -1) {
		if (fchmod(fd, mode) != 0) {
			fprintf(stderr, "do_chmod: set mode '%d' for file '%s' error: %s\n",
				mode, path, strerror(errno));

			return FALSE;
		}

		if (close(fd) != 0) {
			fprintf(stderr, "do_chmod: close file '%s' error: %s\n",
				path, strerror(errno));

			return FALSE;
		}
	} else {
		fprintf(stderr, "do_chmod: open file '%s' error: %s\n",
			path, strerror(errno));

		return FALSE;
	}

	return TRUE;
}

BOOL path_exists(const char *path) {
	struct stat st;

	memset(&st, 0, sizeof(st));

	return stat(path, &st) == 0;
}

static BOOL wait_srd(void) {
	unsigned char try;

	for (try = 0u; try < WAIT_TRY_THRESHOLD; try++) {
		if (path_exists(srd_dir_path)) {
			return TRUE;
		}

		sleep(WAIT_DELAY);
	}

	return FALSE;
}

static char *detect_system_dir(void) {
	unsigned char try;
	char *last_work_dir = NULL;
	unsigned char match_count = 0;

	for (try = 0u; try < WAIT_TRY_THRESHOLD; try++) {
		size_t wd_index;
		char *current_work_dir = NULL;

		for (wd_index = 0ul; wd_index < PSB_SYSTEM_DIRS_COUNT; wd_index++) {
			char *work_dir = psb_system_dirs[wd_index];

			if (path_exists(work_dir)) {
				current_work_dir = work_dir;

				break;
			}
		}

		if (last_work_dir != NULL && current_work_dir == last_work_dir) {
			if (match_count == DETECT_WD_NEED_MATCH) {
				return current_work_dir;
			} else {
				match_count += 1u;
			}
		} else {
			last_work_dir = current_work_dir;
			match_count = 0u;
		}

		sleep(WAIT_DELAY);
	}

	return NULL;
}

static BOOL busybox_install(void) {
	char *bb_argv[] = {path_bb, "--install", "-s", path_bin, NULL};

	if (execvp(path_bb, bb_argv) == 0) {
		return TRUE;
	} else {
		fprintf(stderr, "Error installing '%s' to '%s': %s\n",
			path_bb, path_bin, strerror(errno));

		return FALSE;
	}
}

static BOOL populate_system_dir(const char *system_dir) {
	snprintf(path_bin, PATH_MAX, "%s/bin", system_dir);
	snprintf(path_etc, PATH_MAX, "%s/etc", system_dir);
	snprintf(path_bb, PATH_MAX, "%s/busybox", path_bin);
	snprintf(path_svc, PATH_MAX, "%s/svc.d", path_etc);
	snprintf(path_cfg, PATH_MAX, "%s/cfg", path_etc);

	if (!path_exists(path_bb)) {
		fprintf(stderr, "Busybox not found at '%s'\n", path_bb);

		return FALSE;
	}

	if (!do_chmod(path_bb, 755)) {
		fprintf(stderr, "Can't set mode for '%s'\n", path_bb);
	}

	CREATE_DIR_SB(path_etc);
	CREATE_DIR_SB(path_svc);
	CREATE_DIR_SB(path_cfg);

	if (!busybox_install()) {
		return FALSE;
	}

	return TRUE;
}

static BOOL runsvdir_fork(const char *system_dir) {
	char new_path_env[ENV_MAX];
	pid_t pid;
	char *busybox_argv[] = {path_bb, "runsvdir", path_svc};
	const char *path_env = getenv(PATH_ENV_NAME);

	if (path_env == NULL) {
		path_env = "/bin";
	}

	snprintf(new_path_env, ENV_MAX, "%s:%s", path_bin, path_env);

	if (
		setenv(PATH_ENV_NAME, new_path_env, 1) != 0 ||
		setenv("SYSDIR", system_dir, 1) != 0 ||
		setenv("BINDIR", path_bin, 1) != 0 ||
		setenv("ETCDIR", path_etc, 1) != 0 ||
		setenv("SVDIR", path_svc, 1) != 0 ||
		setenv("CFGDIR", path_cfg, 1) != 0
	) {
		return FALSE;
	}

	pid = fork();

	if (pid == (pid_t)0) {
		execvp(path_bb, busybox_argv);
		exit(EXIT_SUCCESS);
	}

	waitpid(pid, NULL, 0);

	return TRUE;
}

static BOOL init(void) {
	char *system_dir;

	printf("Waiting for system disks directory...");

	if (wait_srd()) {
		printf("OK\n");
	} else {
		printf("FAIL\n");

		return FALSE;
	}

	printf("Creating symlink '%s' -> '%s'...", srd_dir_path, rd_dir_path);

	if (symlink(srd_dir_path, rd_dir_path) == 0) {
		printf("OK\n");
	} else {
		printf("FAIL\n");
		fprintf(stderr, "%s\n", strerror(errno));

		return FALSE;
	}

	printf("Detecting system directory...");
	system_dir = detect_system_dir();

	if (system_dir != NULL) {
		printf("OK\n");
	} else {
		printf("FAIL\n");
		fprintf(stderr, "Can't detect system directory\n");

		return FALSE;
	}

	if (!populate_system_dir(system_dir)) {
		fprintf(stderr, "Populate '%s' directory error\n", system_dir);

		return FALSE;
	}

	return runsvdir_fork(system_dir);
}

int main(int argc, char *argv[]){
	pid_t pid;

  argv[0] = "/oldinit";
	pid = fork();

	if (pid == (pid_t)0) {
		if (init()) {
			return EXIT_SUCCESS;
		} else {
			return EXIT_FAILURE;
		}
	}

  execvp(argv[0], argv);

	return EXIT_SUCCESS;
}
