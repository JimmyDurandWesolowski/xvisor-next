#include <vmm_stdio.h>
#include <linux/linkage.h>

asmlinkage void __div0(void)
{
	vmm_printf("Division by zero in hypervisor.\n");
}
