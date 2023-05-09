#include <linux/syscalls.h>
#include "mktrace.h"

SYSCALL_DEFINE1(moker_tracing, unsigned int, enable)
{
	printk("MOKER: moker_tracing:[%d][%d]\n", (int) enable, current->pid);
	return sys_moker_tracing(enable);
}

int sys_moker_tracing (unsigned int enable){

#ifdef CONFIG_MOKER_TRACING
	printk("MOKER: sys_moker_tracing:[%d][%d]\n", (int) enable, current->pid);
	trace_enable(enable);
#endif

return 0;
}
