#include <stdlib.h>
#include <fenv.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "init.h"

static const char *PSB_SYSTEM_DIRS[] = {
  "/system/ram/disks/usb1/" SYSDIR_SUFFIX,
  "/system/ram/disks/usb1-part1/" SYSDIR_SUFFIX,
  "/system/ram/disks/usb1-part2/" SYSDIR_SUFFIX,
  "/system/ram/disks/usb1-part3/" SYSDIR_SUFFIX,
  "/system/ram/disks/usb1-part4/" SYSDIR_SUFFIX,
  "/system/ram/disks/disk1/" SYSDIR_SUFFIX,
  "/system/ram/disks/disk2/" SYSDIR_SUFFIX,
  "/flash/rw/disk/pub/" SYSDIR_SUFFIX,
  "/flash/rw/disk/flash/rw/disk/pub/" SYSDIR_SUFFIX
};
static const char SYSROOT_PATH[] = "/system";
static const char SRD_DIR_PATH[] = "/system/ram/disks";
static const char RD_DIR_PATH[] = "/ram/disks";
static const size_t SYSROOT_PATH_LEN = sizeof(SYSROOT_PATH) - 1;
static const size_t PSB_SYSTEM_DIRS_COUNT = sizeof(PSB_SYSTEM_DIRS) / sizeof(*PSB_SYSTEM_DIRS);
static const char *PATH_ENV_NAME = "PATH";

static char path_bin[PATH_MAX];
static char path_etc[PATH_MAX];
static char path_tmp[PATH_MAX];
static char path_dev[PATH_MAX];
static char path_pts[PATH_MAX];
static char path_proc[PATH_MAX];
static char path_sys[PATH_MAX];
static char path_bb[PATH_MAX];
static char path_sh[PATH_MAX];
static char path_bb_upg[PATH_MAX];
static char path_mikrotik[PATH_MAX];
static char path_svc[PATH_MAX];
static char path_cfg[PATH_MAX];
static size_t system_dir_len = 0;

#define PATH_WORKTRUNC(PATH) ((char *)PATH + system_dir_len)
#define PATH_SYSTRUNC(PATH) ((char *)PATH + SYSROOT_PATH_LEN)

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
    if (path_exists(SRD_DIR_PATH)) {
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
      const char *work_dir = PSB_SYSTEM_DIRS[wd_index];

      if (path_exists(work_dir)) {
        current_work_dir = (char *)work_dir; /* TODO: Remove dirty const */

        break;
      }
    }

    if (last_work_dir != NULL && current_work_dir == last_work_dir) {
      if (match_count == DETECT_WD_NEED_MATCH) {
        system_dir_len = strnlen(current_work_dir, PATH_MAX);

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

static void mount_bind(const char *src, const char *dst) {
  const char *bb_argv[] = {path_bb, "mount", "-o", "bind", src, dst, NULL};
  pid_t pid = fork();

  if (pid == 0) {
    if (execvp(path_bb, (char *const *)bb_argv) != 0) {
      fprintf(stderr, "Error mounting '%s' to '%s': %s\n",
        src, dst, strerror(errno));
    }

    exit(0);
  } else {
    waitpid(pid, NULL, 0);
  }
}

static void mount_fs(const char *type, const char *src, const char *dst) {
  const char *bb_argv[] = {path_bb, "mount", "-t", type, src, dst, NULL};
  pid_t pid = fork();

  if (pid == 0) {
    if (execvp(path_bb, (char * const*)bb_argv) != 0) {
      fprintf(stderr, "Error mounting '%s' to '%s': %s\n",
        src, dst, strerror(errno));
    }

    exit(0);
  } else {
    waitpid(pid, NULL, 0);
  }
}

static void busybox_uninstall(void) {
  char *bb_argv[] = {path_bb, "find", path_bin, "-samefile", path_sh, "-delete", NULL};
  pid_t pid = fork();

  if (pid == 0) {
    if (execvp(path_bb, bb_argv) != 0) {
      fprintf(stderr, "Error uninstalling '%s': %s\n",
        path_bb, strerror(errno));
    }

    exit(0);
  } else {
    waitpid(pid, NULL, 0);
  }
}

static void busybox_install(void) {
  char *bb_argv[] = {path_bb, "--install", path_bin, NULL};
  pid_t pid = fork();

  if (pid == 0) {
    if (execvp(path_bb, bb_argv) != 0) {
      fprintf(stderr, "Error installing '%s' to '%s': %s\n",
        path_bb, path_bin, strerror(errno));
    }

    exit(0);
  } else {
    waitpid(pid, NULL, 0);
  }
}

static BOOL populate_system_dir(const char *system_dir) {
  char path_buf[PATH_MAX];
  BOOL is_upgrading = FALSE;

  snprintf(path_bin, PATH_MAX, "%s/bin", system_dir);
  snprintf(path_etc, PATH_MAX, "%s/etc", system_dir);
  snprintf(path_tmp, PATH_MAX, "%s/tmp", system_dir);
  snprintf(path_dev, PATH_MAX, "%s/dev", system_dir);
  snprintf(path_pts, PATH_MAX, "%s/dev/pts", system_dir);
  snprintf(path_proc, PATH_MAX, "%s/proc", system_dir);
  snprintf(path_sys, PATH_MAX, "%s/sys", system_dir);
  snprintf(path_mikrotik, PATH_MAX, "%s/mikrotik", system_dir);
  snprintf(path_bb, PATH_MAX, "%s/bin/busybox", system_dir);
  snprintf(path_sh, PATH_MAX, "%s/bin/sh", system_dir);
  snprintf(path_bb_upg, PATH_MAX, "%s/bin/busybox.upgrade", system_dir);
  snprintf(path_svc, PATH_MAX, "%s/etc/svc.d", system_dir);
  snprintf(path_cfg, PATH_MAX, "%s/etc/cfg", system_dir);

  if (path_exists(path_bb_upg)) {
    if (rename(path_bb_upg, path_bb) == 0) {
      is_upgrading = TRUE;
    } else {
      fprintf(stderr, "Busybox upgrade error '%s' -> '%s': %s\n",
        path_bb_upg, path_bb, strerror(errno));
    }
  }

  if (!path_exists(path_bb)) {
    fprintf(stderr, "Busybox not found at '%s'\n", path_bb);

    return FALSE;
  }

  if (!do_chmod(path_bb, 0755)) {
    fprintf(stderr, "Can't set mode for '%s'\n", path_bb);

    return FALSE;
  }

  CREATE_DIR_SB(path_etc);
  CREATE_DIR_SB(path_tmp);
  CREATE_DIR_SB(path_dev);
  CREATE_DIR_SB(path_proc);
  CREATE_DIR_SB(path_sys);
  /* CREATE_DIR_SB(path_mikrotik); */
  CREATE_DIR_SB(path_svc);
  CREATE_DIR_SB(path_cfg);

  if (is_upgrading) {
    busybox_uninstall();
  }

  do_chmod(path_tmp, 0777);
  busybox_install();
  snprintf(path_buf, PATH_MAX, "%s/etc/svc.d/telnetd/run", system_dir);
  do_chmod(path_buf, 0755);
  snprintf(path_buf, PATH_MAX, "%s/etc/svc.d/telnetd/check", system_dir);
  do_chmod(path_buf, 0755);
  snprintf(path_buf, PATH_MAX, "%s/etc/passwd", system_dir);
  do_chmod(path_buf, 0644);
  snprintf(path_buf, PATH_MAX, "%s/etc/shadow", system_dir);
  do_chmod(path_buf, 0640);

  /*
  mount_bind(SYSROOT_PATH, path_mikrotik);
  mount_fs("devtmpfs", "sysdev", path_dev);
  CREATE_DIR_SB(path_pts);
  mount_fs("devpts", "syspts", path_pts);
  mount_fs("proc", "sysproc", path_proc);
  mount_fs("sysfs", "syssys", path_sys);
  */

  return TRUE;
}

static BOOL runsvdir_fork(char *system_dir) {
  char new_path_env[ENV_MAX];
  pid_t pid;
  char *busybox_argv[] = {path_bb, "chroot", (char *)SYSROOT_PATH, PATH_SYSTRUNC(path_bb), "runsvdir", PATH_SYSTRUNC(path_svc), NULL};
  const char *path_env = getenv(PATH_ENV_NAME);

  if (path_env == NULL) {
    path_env = "/bin";
  }

  snprintf(new_path_env, ENV_MAX, "%s:%s",
    PATH_SYSTRUNC(path_bin), path_env);

  if (
    setenv(PATH_ENV_NAME, new_path_env, 1) != 0 ||
    setenv("SYSDIR", PATH_SYSTRUNC(system_dir), 1) != 0 ||
    setenv("BINDIR", PATH_SYSTRUNC(path_bin), 1) != 0 ||
    setenv("ETCDIR", PATH_SYSTRUNC(path_etc), 1) != 0 ||
    setenv("SVDIR", PATH_SYSTRUNC(path_svc), 1) != 0 ||
    setenv("CFGDIR", PATH_SYSTRUNC(path_cfg), 1) != 0
  ) {
    return FALSE;
  }

  pid = fork();

  if (pid == (pid_t)0) {
    execvp(busybox_argv[0], busybox_argv);
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

  printf("Creating symlink '%s' -> '%s'...", SRD_DIR_PATH, RD_DIR_PATH);

  if (symlink(SRD_DIR_PATH, RD_DIR_PATH) == 0 || errno == EEXIST) {
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

int main(int argc, char *argv[]) {
  pid_t pid;

  (void)mount_fs;
  (void)mount_bind;

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
