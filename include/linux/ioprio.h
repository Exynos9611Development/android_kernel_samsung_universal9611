/* SPDX-License-Identifier: GPL-2.0 */
#ifndef IOPRIO_H
#define IOPRIO_H

#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/iocontext.h>

#include <uapi/linux/ioprio.h>

/*
 * if process has set io priority explicitly, use that. if not, convert
 * the cpu scheduler nice value to an io priority
 */
static inline int task_nice_ioprio(struct task_struct *task)
{
	return (task_nice(task) + 20) / 5;
}

/*
 * This is for the case where the task hasn't asked for a specific IO class.
 * Check for idle and rt task process, and return appropriate IO class.
 */
static inline int task_nice_ioclass(struct task_struct *task)
{
	if (task->policy == SCHED_IDLE)
		return IOPRIO_CLASS_IDLE;
	else if (task_is_realtime(task))
		return IOPRIO_CLASS_RT;
	else
		return IOPRIO_CLASS_BE;
}

/*
 * For inheritance, return the highest of the two given priorities
 */
extern int ioprio_best(unsigned short aprio, unsigned short bprio);

extern int set_task_ioprio(struct task_struct *task, int ioprio);

#endif
