#include "seteuid.h"
#include "../base.h"
#include "../errno.h"
#include <linux-syscalls/linux.h>

long sys_seteuid(int euid)
{
	int ret;

	ret = LINUX_SYSCALL3(__NR_setresuid, -1, euid, -1);
	if (ret < 0)
		ret = errno_linux_to_bsd(ret);

	return ret;
}

