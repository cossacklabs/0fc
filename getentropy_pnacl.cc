#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"{
#endif

static int gotdata(char *buf, size_t len)
{
	char	any_set = 0;
	size_t	i;

	for (i = 0; i < len; ++i)
		any_set |= buf[i];
	if (any_set == 0)
		return -1;
	return 0;
}

int getentropy(void *buf, size_t len)
{
	struct stat st;
	size_t i;
	int flags;
	int save_errno = errno;
	FILE* fd = fopen("/dev/urandom", "r");
	if (!fd) {
	    fprintf(stderr, "Unable to open /dev/urandom.\n");
	    return -1;
	}
	ssize_t bytes = fread(buf, 1, len, fd);
	if (bytes != len) {
	    fprintf(stderr, "Unable to read %d bytes.\n", len);
	    fclose(fd);
	    return -1;
	}
	fclose(fd);
	if (gotdata((char*)buf, len) == 0) {
		errno = save_errno;
		return 0;		/* satisfied */
	}
	fprintf(stderr, "other error");
	return -1;
}

#ifdef __cplusplus
}
#endif