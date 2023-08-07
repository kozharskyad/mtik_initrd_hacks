#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

typedef unsigned char BOOL;

#ifdef FALSE
#undef FALSE
#endif
#define FALSE ((BOOL)0u)

#ifdef TRUE
#undef TRUE
#endif
#define TRUE (!FALSE)

#ifdef PATH_MAX
#undef PATH_MAX
#endif
#define PATH_MAX (256)

#ifdef ENV_MAX
#undef ENV_MAX
#endif
#define ENV_MAX 1024

#ifdef WAIT_TRY_THRESHOLD
#undef WAIT_TRY_THRESHOLD
#endif
#define WAIT_TRY_THRESHOLD 60u

#ifdef WAIT_DELAY
#undef WAIT_DELAY
#endif
#define WAIT_DELAY 2

#ifdef DETECT_WD_NEED_MATCH
#undef DETECT_WD_NEED_MATCH
#endif
#define DETECT_WD_NEED_MATCH 3u

#ifdef SYSDIR_SUFFIX
#undef SYSDIR_SUFFIX
#endif
#define SYSDIR_SUFFIX "system-v3"

#ifdef CREATE_DIR_SB
#undef CREATE_DIR_SB
#endif

BOOL path_exists(const char *path);

#define CREATE_DIR_SB(PATH) \
	if (!path_exists(PATH) && mkdir(PATH, 0755) != 0) { \
		fprintf(stderr, "Cannot create '%s': %s\n", PATH, strerror(errno)); \
    \
		return FALSE; \
	}
