/*
 * Performance CPU Finder
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <trace/events/ems.h>

#include "../sched.h"
#include "ems.h"

/*
 * Currently, PCF is composed of a selection algorithm based on distributed
 * processing, for example, selecting idle cpu or cpu with biggest spare
 * capacity. Although the current algorithm may suffice, it is necessary to
 * examine a selection algorithm considering cache hot and migration cost.
 */
int select_perf_cpu(struct task_struct *p)
{
	int cpu;
	unsigned long best_perf_cap_orig = 0;
	unsigned long max_spare_cap = 0;
	int best_perf_cstate = INT_MAX;
	unsigned long best_perf_idle_util = ULONG_MAX;
	unsigned long new_util;
	int best_perf_cpu = -1;
	int backup_cpu = -1;

	rcu_read_lock();

	for_each_cpu_and(cpu, &p->cpus_allowed, cpu_active_mask) {
		unsigned long capacity_orig = capacity_orig_of(cpu);
		unsigned long wake_util;

		/*
		 * A) Find best performance cpu.
		 *
		 * If the impact of cache hot and migration cost are excluded,
		 * distributed processing is the best way to achieve performance.
		 * To maximize performance, the idle cpu with the highest
		 * performance is selected first. If there are more than two idle
		 * cpus with the highest performance, choose the cpu with the
		 * shallowest idle state for fast reactivity.  Among CPUs at the
		 * same idle depth, prefer the one with the lower utilisation so
		 * the new task gets the most headroom and the frequency does not
		 * need to scale up as quickly.
		 */
		if (idle_cpu(cpu)) {
			int idle_idx = idle_get_state_idx(cpu_rq(cpu));
			unsigned long util = cpu_util_wake(cpu, p);

			/* find biggest capacity cpu */
			if (capacity_orig < best_perf_cap_orig)
				continue;

			/*
			 * if we find a better-performing cpu, re-initialize
			 * best_perf_cstate and best_perf_idle_util
			 */
			if (capacity_orig > best_perf_cap_orig) {
				best_perf_cap_orig = capacity_orig;
				best_perf_cstate = INT_MAX;
				best_perf_idle_util = ULONG_MAX;
			}

			/* find shallowest idle state cpu */
			if (idle_idx > best_perf_cstate)
				continue;

			/*
			 * At the same idle depth, prefer the less loaded CPU.
			 * This distributes the work evenly across idle CPUs in
			 * the biggest cluster and avoids frequency spikes.
			 */
			if (idle_idx == best_perf_cstate &&
			    util >= best_perf_idle_util)
				continue;

			/* Keep track of best idle CPU */
			best_perf_cstate = idle_idx;
			best_perf_idle_util = util;
			best_perf_cpu = cpu;
			continue;
		}

		/*
		 * B) Find backup performance cpu.
		 *
		 * Backup cpu also adopts distributed processing. In the absence
		 * of idle cpu, it is difficult to expect reactivity, so select
		 * the cpu with the biggest spare capacity to handle the most
		 * computations. Since a high performance cpu has a large capacity,
		 * cpu having a high performance is likely to be selected.
		 *
		 * Use task_util_est() rather than a raw util delta so that tasks
		 * with UTIL_EST history (e.g. camera threads being rescheduled)
		 * are accounted for correctly.  Also honour boosted_task_util()
		 * so that a schedtune-boosted task is not placed on a CPU whose
		 * effective capacity would be exceeded after the boost is applied.
		 */
		wake_util = cpu_util_wake(cpu, p);
		new_util = wake_util + task_util_est(p);
		new_util = max(new_util, boosted_task_util(p));
		if (new_util > capacity_orig)
			continue;
		if ((capacity_orig - wake_util) < max_spare_cap)
			continue;

		max_spare_cap = capacity_orig - wake_util;
		backup_cpu = cpu;
	}

	rcu_read_unlock();

	trace_ems_select_perf_cpu(p, best_perf_cpu, backup_cpu);
	if (best_perf_cpu == -1)
		return backup_cpu;

	return best_perf_cpu;
}
