#include "getsockname.h"
#include "../base.h"
#include "../errno.h"
#include <linux-syscalls/linux.h>
#include "duct.h"

long sys_getsockname(int fd, void* asa, int* socklen)
{
	int ret;
	struct sockaddr_fixup* fixed;

#ifdef __NR_socketcall
	ret = LINUX_SYSCALL(__NR_socketcall, LINUX_SYS_GETSOCKNAME,
			((long[6]) { fd, asa, socklen }));
#else
	ret = LINUX_SYSCALL(__NR_getsockname, fd, asa, socklen);
#endif

	if (ret < 0)
		ret = errno_linux_to_bsd(ret);
	else
	{
		fixed = (struct sockaddr_fixup*) asa;
		fixed->bsd_family = sfamily_linux_to_bsd(fixed->linux_family);
		fixed->bsd_length = *socklen;
	}

	return ret;
}

