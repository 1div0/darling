/*
 * Darling Mach Linux Kernel Module
 * Copyright (C) 2015 Lubos Dolezel
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <mach/mach_types.h>
#include "../mach_includes.h"
#include "../ipc_server.h"
#include "../darling_task.h"
#include "stub.h"
#include <linux/sched.h>
#include <linux/uaccess.h>
#include "thread_act.h"

struct thread_private
{
	pid_t thread_id;
};

static
void __thread_free(server_port_t* port)
{
	kfree(port->private_data);
}

extern struct mig_subsystem thread_act_subsystem;
void ipc_port_make_thread(darling_mach_port_t* port)
{
	struct thread_private* priv;
	
	port->is_server_port = true;
	port->server_port.subsystem = (mig_subsystem_t) &thread_act_subsystem;
	port->server_port.cb_free = __thread_free;
	
	priv = (struct thread_private*) kmalloc(sizeof(struct thread_private),
										GFP_KERNEL);
	
	port->server_port.private_data = priv;
	priv->thread_id = current->pid;
}

pid_t get_thread_pid(darling_mach_port_t* port)
{
	struct thread_private* priv;
	
	if (port->server_port.subsystem != (mig_subsystem_t) &thread_act_subsystem)
		return 0;
	
	priv = (struct thread_private*) port->server_port.private_data;
	return priv->thread_id;
}

kern_return_t thread_terminate
(
	thread_act_t target_act
)
{
	kern_return_t ret = KERN_SUCCESS;
	mach_task_t* task;
	darling_mach_port_right_t* right = NULL;

	task = darling_task_get_current();
	ipc_space_lock(&task->namespace);

	right = ipc_space_lookup(&task->namespace, target_act);
	if (right == NULL)
	{
		ret = KERN_INVALID_RIGHT;
		goto err;
	}
	if (!PORT_IS_VALID(right->port)
			|| right->port != darling_task_lookup_thread(task, current->pid))
	{
		ret = KERN_INVALID_RIGHT;
		goto err;
	}

	darling_task_deregister_thread(task, right->port);
	ipc_port_put(right->port);

	ipc_space_right_put(&task->namespace, target_act);
	right = NULL;
   
err:
	if (right != NULL)
		ipc_port_unlock(right->port);
	ipc_space_unlock(&task->namespace);
	return ret;
}

kern_return_t act_get_state
(
	thread_act_t target_act,
	int flavor,
	thread_state_t old_state,
	mach_msg_type_number_t *old_stateCnt
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t act_set_state
(
	thread_act_t target_act,
	int flavor,
	thread_state_t new_state,
	mach_msg_type_number_t new_stateCnt
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_get_state
(
	thread_act_t target_act,
	thread_state_flavor_t flavor,
	thread_state_t old_state,
	mach_msg_type_number_t *old_stateCnt
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_set_state
(
	thread_act_t target_act,
	thread_state_flavor_t flavor,
	thread_state_t new_state,
	mach_msg_type_number_t new_stateCnt
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_suspend
(
	thread_act_t target_act
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_resume
(
	thread_act_t target_act
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_abort
(
	thread_act_t target_act
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_abort_safely
(
	thread_act_t target_act
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_depress_abort
(
	thread_act_t thread
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_get_special_port
(
	thread_act_t thr_act,
	int which_port,
	mach_port_t *special_port
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_set_special_port
(
	thread_act_t thr_act,
	int which_port,
	mach_port_t special_port
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_info
(
	thread_act_t target_act,
	thread_flavor_t flavor,
	thread_info_t thread_info_out,
	mach_msg_type_number_t *thread_info_outCnt
)
{
	mach_task_t* task;
	darling_mach_port_right_t* right = NULL;
	struct task_struct* ltask = NULL;
	pid_t pid;
	kern_return_t ret = KERN_SUCCESS;

	task = darling_task_get_current();
	ipc_space_lock(&task->namespace);

	right = ipc_space_lookup_unlocked(&task->namespace, target_act);
	if (right == NULL)
	{
		debug_msg("thread_info: right %d not found", target_act);
		ret = KERN_INVALID_RIGHT;
		goto err;
	}
	
	pid = get_thread_pid(right->port);
	ltask = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);
	
	if (ltask == NULL)
	{
		ret = KERN_FAILURE;
		goto err;
	}
   
	if (flavor == THREAD_IDENTIFIER_INFO)
	{
		thread_identifier_info_data_t data;
		
		data.thread_handle = ltask->pid;
		data.dispatch_qaddr = 0;
		
		data.thread_id = data.thread_handle;
		data.thread_id |= (ltask->start_time & 0xffffffff) << 32;
		
		memcpy(thread_info_out, &data, sizeof(data));
		*thread_info_outCnt = THREAD_IDENTIFIER_INFO_COUNT;
	}
	else
	{
		debug_msg("Unsupported thread_info flavor: %d", flavor);
		UNIMPL_MIG_CALL();
		ret = KERN_NOT_SUPPORTED;
	}
	
err:
	ipc_space_unlock(&task->namespace);
	
	return ret;
}

kern_return_t thread_set_exception_ports
(
	thread_act_t thread,
	exception_mask_t exception_mask,
	mach_port_t new_port,
	exception_behavior_t behavior,
	thread_state_flavor_t new_flavor
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_get_exception_ports
(
	thread_act_t thread,
	exception_mask_t exception_mask,
	exception_mask_array_t masks,
	mach_msg_type_number_t *masksCnt,
	exception_handler_array_t old_handlers,
	exception_behavior_array_t old_behaviors,
	exception_flavor_array_t old_flavors
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_swap_exception_ports
(
	thread_act_t thread,
	exception_mask_t exception_mask,
	mach_port_t new_port,
	exception_behavior_t behavior,
	thread_state_flavor_t new_flavor,
	exception_mask_array_t masks,
	mach_msg_type_number_t *masksCnt,
	exception_handler_array_t old_handlers,
	exception_behavior_array_t old_behaviors,
	exception_flavor_array_t old_flavors
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t _kernelrpc_thread_policy
(
	thread_act_t thr_act,
	policy_t policy,
	policy_base_t base,
	mach_msg_type_number_t baseCnt,
	boolean_t set_limit
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t _kernelrpc_thread_policy_set
(
	thread_act_t thread,
	thread_policy_flavor_t flavor,
	thread_policy_t policy_info,
	mach_msg_type_number_t policy_infoCnt
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_policy_get
(
	thread_act_t thread,
	thread_policy_flavor_t flavor,
	thread_policy_t policy_info,
	mach_msg_type_number_t *policy_infoCnt,
	boolean_t *get_default
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_sample
(
	thread_act_t thread,
	mach_port_t reply
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t etap_trace_thread
(
	thread_act_t target_act,
	boolean_t trace_status
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_assign
(
	thread_act_t thread,
	processor_set_t new_set
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_assign_default
(
	thread_act_t thread
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_get_assignment
(
	thread_act_t thread,
	processor_set_name_t *assigned_set
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t _kernelrpc_thread_set_policy
(
	thread_act_t thr_act,
	processor_set_t pset,
	policy_t policy,
	policy_base_t base,
	mach_msg_type_number_t baseCnt,
	policy_limit_t limit,
	mach_msg_type_number_t limitCnt
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_get_mach_voucher
(
	thread_act_t thr_act,
	mach_voucher_selector_t which,
	ipc_voucher_t *voucher
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_set_mach_voucher
(
	thread_act_t thr_act,
	ipc_voucher_t voucher
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

kern_return_t thread_swap_mach_voucher
(
	thread_act_t thr_act,
	ipc_voucher_t new_voucher,
	ipc_voucher_t *old_voucher
)
{
	UNIMPL_MIG_CALL();
	return KERN_NOT_SUPPORTED;
}

