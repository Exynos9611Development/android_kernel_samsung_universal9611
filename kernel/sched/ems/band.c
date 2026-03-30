/*
 * thread group band
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ems.h>
#include <linux/sched/signal.h>
#include <trace/events/ems.h>

#include "../sched.h"
#include "ems.h"

static struct task_band *lookup_band(struct task_struct *p)
{
	struct task_band *band;

	rcu_read_lock();
	band = rcu_dereference(p->band);
	rcu_read_unlock();

	if (!band)
		return NULL;

	return band;
}

int band_play_cpu(struct task_struct *p)
{
	struct task_band *band;
	unsigned long task_util_val = task_util_est(p);
	int cpu;
	int best_idle_cpu = -1;
	int min_cpu = -1;
	unsigned long min_util = ULONG_MAX;
	int best_idle_cstate = INT_MAX;
	unsigned long best_idle_util = ULONG_MAX;

	band = lookup_band(p);
	if (!band)
		return -1;

	for_each_cpu(cpu, &band->playable_cpus) {
		unsigned long capacity_orig = capacity_orig_of(cpu);
		unsigned long wake_util = cpu_util_wake(cpu, p);
		unsigned long new_util = wake_util + task_util_val;

		new_util = max(new_util, boosted_task_util(p));

		/* Skip CPUs that cannot accommodate this task */
		if (new_util > capacity_orig)
			continue;

		if (idle_cpu(cpu)) {
			/*
			 * Prefer the shallowest idle state so the CPU
			 * wakes up fastest and the task starts sooner.
			 * Among CPUs at the same idle depth prefer the
			 * one with the lower utilisation — consistent
			 * with pcf.c and service.c — so that video
			 * threads land on the most headroom-rich CPU
			 * and avoid unnecessary frequency ramp-ups.
			 */
			int cstate = idle_get_state_idx(cpu_rq(cpu));

			if (cstate > best_idle_cstate)
				continue;
			if (cstate == best_idle_cstate &&
			    wake_util >= best_idle_util)
				continue;

			best_idle_cstate = cstate;
			best_idle_util = wake_util;
			best_idle_cpu = cpu;
			continue;
		}

		/*
		 * Use cpu_util_wake() + task_util_est() for the non-idle
		 * candidate rather than plain cpu_util() so that the task's
		 * own blocked contribution is not double-counted on prev_cpu.
		 */
		if (new_util < min_util) {
			min_cpu = cpu;
			min_util = new_util;
		}
	}

	/* Idle CPU wins over an active one */
	if (cpu_selected(best_idle_cpu))
		return best_idle_cpu;

	return min_cpu;
}

static void pick_playable_cpus(struct task_band *band)
{
	int cpu, last_valid_cpu = -1;

	cpumask_clear(&band->playable_cpus);

	/*
	 * Find the first coregroup whose total capacity is large enough to
	 * accommodate twice the band's current utilization.  Using 2x gives
	 * the same headroom as the previous hard-coded "up-threshold * 2"
	 * thresholds while being derived from the actual hardware topology
	 * rather than from device-specific magic numbers.
	 */
	for_each_cpu(cpu, cpu_active_mask) {
		unsigned long cpu_capacity;
		int ncpus;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cpu_capacity = get_cpu_max_capacity(cpu);
		if (!cpu_capacity)
			continue;

		last_valid_cpu = cpu;
		ncpus = cpumask_weight(cpu_coregroup_mask(cpu));

		if ((band->util << 1) <= cpu_capacity * ncpus) {
			cpumask_and(&band->playable_cpus, cpu_online_mask,
				    cpu_coregroup_mask(cpu));
			return;
		}
	}

	/* Fallback: use the fastest available coregroup */
	if (last_valid_cpu >= 0)
		cpumask_and(&band->playable_cpus, cpu_online_mask,
			    cpu_coregroup_mask(last_valid_cpu));
}

static unsigned long out_of_time = 100000000;	/* 100ms */

/* This function should be called protected with band->lock */
static void __update_band(struct task_band *band, unsigned long now)
{
	struct task_struct *task;
	unsigned long util_sum = 0;

	list_for_each_entry(task, &band->members, band_members) {
		if (now - task->se.avg.last_update_time > out_of_time)
			continue;
		/*
		 * Use task_util_est() rather than task_util() so that the
		 * band's utilization accounts for UTIL_EST history.  A band
		 * member that has just woken up may have a decayed util_avg
		 * close to zero, causing pick_playable_cpus() to assign the
		 * band to the smallest coregroup and then immediately stall
		 * when the task's true load is revealed.  task_util_est() uses
		 * the EWMA / enqueued history and gives a stable, higher
		 * estimate from the first wake-up.
		 */
		util_sum += task_util_est(task);
	}

	band->util = util_sum;
	band->last_update_time = now;

	pick_playable_cpus(band);

	if (list_empty(&band->members))
		return;

	trace_ems_update_band(band->id, band->util, band->member_count,
		*(unsigned int *)cpumask_bits(&band->playable_cpus));
}

static int update_interval = 40000000;	/* 40ms */

void update_band(struct task_struct *p, long old_util)
{
	struct task_band *band;
	unsigned long now = (unsigned long)local_clock();

	band = lookup_band(p);
	if (!band)
		return;

	/*
	 * Updates the utilization of the band only when it has been enough time
	 * to update the utilization of the band, or when the utilization of the
	 * task changes abruptly.
	 */
	if (now - band->last_update_time >= update_interval ||
	    (old_util >= 0 && abs(old_util - task_util_est(p)) > (SCHED_CAPACITY_SCALE >> 4))) {
		raw_spin_lock(&band->lock);
		__update_band(band, now);
		raw_spin_unlock(&band->lock);
	}
}

#define MAX_NUM_BAND_ID		20
static struct task_band *bands[MAX_NUM_BAND_ID];

DEFINE_RWLOCK(band_rwlock);

#define band_playing(band)	(band->tgid >= 0)
static void join_band(struct task_struct *p)
{
	struct task_band *band;
	int pos, empty = -1;
	char event[30] = "join band";

	if (lookup_band(p))
		return;

	write_lock(&band_rwlock);

	/*
	 * Find the band assigned to the tasks's thread group in the
	 * band pool. If there is no band assigend to thread group, it
	 * indicates that the task is the first one in the thread group
	 * to join the band. In this case, assign the first empty band
	 * in the band pool to the thread group.
	 */
	for (pos = 0; pos < MAX_NUM_BAND_ID; pos++) {
		band = bands[pos];

		if (!band_playing(band)) {
			if (empty < 0)
				empty = pos;
			continue;
		}

		if (p->tgid == band->tgid)
			break;
	}

	/* failed to find band, organize the new band */
	if (pos == MAX_NUM_BAND_ID) {
		if (unlikely(empty < 0)) {
			/* All band slots occupied; drop silently */
			write_unlock(&band_rwlock);
			return;
		}
		band = bands[empty];
	}

	/* Re-check p->band under the write lock to close the TOCTOU window */
	if (p->band) {
		write_unlock(&band_rwlock);
		return;
	}

	raw_spin_lock(&band->lock);
	if (!band_playing(band))
		band->tgid = p->tgid;
	list_add(&p->band_members, &band->members);
	rcu_assign_pointer(p->band, band);
	band->member_count++;
	trace_ems_manage_band(p, band->id, event);

	__update_band(band, (unsigned long)local_clock());
	raw_spin_unlock(&band->lock);

	write_unlock(&band_rwlock);
}

static void leave_band(struct task_struct *p)
{
	struct task_band *band;
	char event[30] = "leave band";

	if (!lookup_band(p))
		return;

	write_lock(&band_rwlock);
	band = p->band;
	if (!band) {
		write_unlock(&band_rwlock);
		return;
	}

	raw_spin_lock(&band->lock);
	list_del_init(&p->band_members);
	rcu_assign_pointer(p->band, NULL);
	band->member_count--;
	trace_ems_manage_band(p, band->id, event);

	/* last member of band, band split up */
	if (list_empty(&band->members)) {
		band->tgid = -1;
		cpumask_clear(&band->playable_cpus);
	}

	__update_band(band, (unsigned long)local_clock());
	raw_spin_unlock(&band->lock);

	write_unlock(&band_rwlock);
}

void sync_band(struct task_struct *p, bool join)
{
	if (join)
		join_band(p);
	else
		leave_band(p);
}

void newbie_join_band(struct task_struct *newbie)
{
	unsigned long flags;
	struct task_band *band;
	struct task_struct *leader = newbie->group_leader;
	char event[30] = "newbie join band";

	if (thread_group_leader(newbie))
		return;

	write_lock_irqsave(&band_rwlock, flags);

	band = lookup_band(leader);
	if (!band || newbie->band) {
		write_unlock_irqrestore(&band_rwlock, flags);
		return;
	}

	raw_spin_lock(&band->lock);
	list_add(&newbie->band_members, &band->members);
	rcu_assign_pointer(newbie->band, band);
	band->member_count++;
	trace_ems_manage_band(newbie, band->id, event);
	raw_spin_unlock(&band->lock);

	write_unlock_irqrestore(&band_rwlock, flags);
}

int alloc_bands(void)
{
	struct task_band *band;
	int pos, ret, i;

	for (pos = 0; pos < MAX_NUM_BAND_ID; pos++) {
		band = kzalloc(sizeof(*band), GFP_KERNEL);
		if (!band) {
			ret = -ENOMEM;
			goto fail;
		}

		band->id = pos;
		band->tgid = -1;
		raw_spin_lock_init(&band->lock);
		INIT_LIST_HEAD(&band->members);
		band->member_count = 0;
		cpumask_clear(&band->playable_cpus);

		bands[pos] = band;
	}

	return 0;

fail:
	for (i = pos - 1; i >= 0; i--) {
		kfree(bands[i]);
		bands[i] = NULL;
	}

	return ret;
}
