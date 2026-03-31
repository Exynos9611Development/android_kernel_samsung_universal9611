/*
 * Services for Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/kobject.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/ems_service.h>
#include <trace/events/ems.h>

#include "../sched.h"
#include "../tune.h"
#include "ems.h"

/**********************************************************************
 *                        Kernel Prefer Perf                          *
 **********************************************************************/
struct plist_head kpp_list[STUNE_GROUP_COUNT];

static bool kpp_en;

int kpp_status(int grp_idx)
{
	if (unlikely(!kpp_en))
		return 0;

	if (grp_idx >= STUNE_GROUP_COUNT)
		return -EINVAL;

	if (plist_head_empty(&kpp_list[grp_idx]))
		return 0;

	return plist_last(&kpp_list[grp_idx])->prio;
}

static DEFINE_SPINLOCK(kpp_lock);

void kpp_request(int grp_idx, struct kpp *req, int value)
{
	unsigned long flags;

	if (unlikely(!kpp_en))
		return;

	if (grp_idx >= STUNE_GROUP_COUNT)
		return;

	if (req->active && req->node.prio == value && req->grp_idx == grp_idx)
		return;

	spin_lock_irqsave(&kpp_lock, flags);

	/*
	 * If the request already added to the list updates the value, remove
	 * the request from the list and add it again.
	 */
	if (req->active)
		plist_del(&req->node, &kpp_list[req->grp_idx]);
	else
		req->active = 1;

	plist_node_init(&req->node, value);
	plist_add(&req->node, &kpp_list[grp_idx]);
	req->grp_idx = grp_idx;

	spin_unlock_irqrestore(&kpp_lock, flags);
}

static void __init init_kpp(void)
{
	int i;

	for (i = 0; i < STUNE_GROUP_COUNT; i++)
		plist_head_init(&kpp_list[i]);

	kpp_en = 1;
}

struct prefer_perf {
	int			boost;
	unsigned int		threshold;
	unsigned int		coregroup_count;
	struct cpumask		*prefer_cpus;
};

static struct prefer_perf *prefer_perf_services;
static int prefer_perf_service_count;

static struct prefer_perf *find_prefer_perf(int boost)
{
	int i;

	for (i = 0; i < prefer_perf_service_count; i++)
		if (prefer_perf_services[i].boost == boost)
			return &prefer_perf_services[i];

	return NULL;
}

static int
select_prefer_cpu(struct task_struct *p, int coregroup_count, struct cpumask *prefer_cpus)
{
	struct cpumask mask;
	int coregroup, cpu;
	unsigned long task_util_val = task_util_est(p);
	unsigned long max_spare_cap = 0;
	int best_perf_cstate = INT_MAX;
	unsigned long best_perf_idle_util = ULONG_MAX;
	int best_perf_cpu = -1;
	int backup_cpu = -1;

	rcu_read_lock();

	for (coregroup = 0; coregroup < coregroup_count; coregroup++) {
		cpumask_and(&mask, &prefer_cpus[coregroup], cpu_active_mask);
		if (cpumask_empty(&mask))
			continue;

		for_each_cpu_and(cpu, &p->cpus_allowed, &mask) {
			unsigned long capacity_orig = capacity_orig_of(cpu);
			unsigned long wake_util = cpu_util_wake(cpu, p);
			unsigned long new_util;

			new_util = wake_util + task_util_val;
			new_util = max(new_util, boosted_task_util(p));

			/* Skip over-capacity CPUs for both idle and active paths */
			if (new_util > capacity_orig)
				continue;

			if (idle_cpu(cpu)) {
				int idle_idx = idle_get_state_idx(cpu_rq(cpu));

				/*
				 * Prefer shallowest idle state for fastest wake-up.
				 * Among equal idle depths prefer the less loaded CPU
				 * (same fix as pcf.c::select_perf_cpu).
				 */
				if (idle_idx > best_perf_cstate)
					continue;

				if (idle_idx == best_perf_cstate &&
				    wake_util >= best_perf_idle_util)
					continue;

				/* Keep track of best idle CPU */
				best_perf_cstate = idle_idx;
				best_perf_idle_util = wake_util;
				best_perf_cpu = cpu;
				continue;
			}

			/*
			 * For active CPUs use spare capacity as the metric but
			 * with the capacity check already applied above so we
			 * never pick a CPU that would overflow after placement.
			 */
			if ((capacity_orig - wake_util) < max_spare_cap)
				continue;

			max_spare_cap = capacity_orig - wake_util;
			backup_cpu = cpu;
		}

		if (cpu_selected(best_perf_cpu))
			break;
	}

	rcu_read_unlock();

	if (best_perf_cpu == -1)
		return backup_cpu;

	return best_perf_cpu;
}

int select_service_cpu(struct task_struct *p)
{
	struct prefer_perf *pp;
	int boost, service_cpu;
	unsigned long util;
	char state[30];

	if (!prefer_perf_services)
		return -1;

	boost = schedtune_prefer_perf(p);
	if (boost <= 0)
		return -1;

	pp = find_prefer_perf(boost);
	if (!pp)
		return -1;

	util = task_util_est(p);
	if (util <= pp->threshold) {
		service_cpu = select_prefer_cpu(p, 1, pp->prefer_cpus);
		strlcpy(state, "light task", sizeof(state));
		goto out;
	}

	if (p->prio <= 110) {
		service_cpu = select_prefer_cpu(p, 1, pp->prefer_cpus);
		strlcpy(state, "high-prio task", sizeof(state));
	} else {
		service_cpu = select_prefer_cpu(p, pp->coregroup_count, pp->prefer_cpus);
		strlcpy(state, "heavy task", sizeof(state));
	}

out:
	trace_ems_prefer_perf_service(p, util, service_cpu, state);
	return service_cpu;
}

static ssize_t show_kpp(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, ret = 0;

	/* shows the prefer_perf value of all schedtune groups */
	for (i = 0; i < STUNE_GROUP_COUNT; i++)
		ret += snprintf(buf + ret, 10, "%d ", kpp_status(i));

	ret += snprintf(buf + ret, 10, "\n");

	return ret;
}

static struct kobj_attribute kpp_attr =
__ATTR(kernel_prefer_perf, 0444, show_kpp, NULL);

static void __init build_prefer_cpus(void)
{
	struct device_node *ems_dn, *dn, *child;
	int index = 0;

	ems_dn = of_find_node_by_name(NULL, "ems");
	if (!ems_dn)
		return;

	dn = of_get_child_by_name(ems_dn, "prefer-perf-service");
	of_node_put(ems_dn);
	if (!dn)
		return;

	prefer_perf_service_count = of_get_child_count(dn);
	if (!prefer_perf_service_count) {
		of_node_put(dn);
		return;
	}

	prefer_perf_services = kcalloc(prefer_perf_service_count,
				sizeof(struct prefer_perf), GFP_KERNEL);
	if (!prefer_perf_services) {
		of_node_put(dn);
		return;
	}

	for_each_child_of_node(dn, child) {
		const char *mask[NR_CPUS];
		int i, proplen;

		if (index >= prefer_perf_service_count) {
			of_node_put(child);
			break;
		}

		of_property_read_u32(child, "boost",
					&prefer_perf_services[index].boost);

		of_property_read_u32(child, "light-task-threshold",
					&prefer_perf_services[index].threshold);

		proplen = of_property_count_strings(child, "prefer-cpus");
		if (proplen < 0)
			goto next;

		prefer_perf_services[index].coregroup_count = proplen;

		of_property_read_string_array(child, "prefer-cpus", mask, proplen);
		prefer_perf_services[index].prefer_cpus = kcalloc(proplen,
						sizeof(struct cpumask), GFP_KERNEL);
		if (!prefer_perf_services[index].prefer_cpus)
			goto next;

		for (i = 0; i < proplen; i++)
			cpulist_parse(mask[i], &prefer_perf_services[index].prefer_cpus[i]);

next:
		index++;
	}

	of_node_put(dn);
}

static int __init init_service(void)
{
	int ret;

	init_kpp();

	build_prefer_cpus();

	ret = sysfs_create_file(ems_kobj, &kpp_attr.attr);
	if (ret)
		pr_err("%s: failed to create sysfs file\n", __func__);

	return 0;
}
late_initcall(init_service);
