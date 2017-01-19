#include "flistxattr.h"
#include "../base.h"
#include "../errno.h"
#include <linux-syscalls/linux.h>

long sys_flistxattr(int fd, char* namebuf, unsigned long size, int options)
{
	int ret;

	ret = LINUX_SYSCALL(__NR_flistxattr, fd, namebuf, size);

	if (ret < 0)
		return errno_linux_to_bsd(ret);

	return ret;
}

