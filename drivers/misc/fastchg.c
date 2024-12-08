/*
 * Author: Chad Froebel <chadfroebel@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Possible values for "force_fast_charge" are:
 *   0 - Disabled (default)
 *   1 - Force faster charge
 */

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/fastchg.h>
#include <linux/string.h>
#include <linux/module.h>

int force_fast_charge = 1;

static int __init get_fastcharge_opt(char *ffc)
{
    if (!ffc)
        return 1;
        
    if (strcmp(ffc, "0") == 0) {
        force_fast_charge = 0;
    } else if (strcmp(ffc, "1") == 0) {
        force_fast_charge = 1;
    } else {
        pr_warn("fastchg: invalid option '%s', using default (%d)\n", 
                ffc, force_fast_charge);
    }
    return 1;
}

__setup("ffc=", get_fastcharge_opt);

static ssize_t force_fast_charge_show(struct kobject *kobj, 
                                     struct kobj_attribute *attr, 
                                     char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", force_fast_charge);
}

static ssize_t force_fast_charge_store(struct kobject *kobj, 
                                      struct kobj_attribute *attr, 
                                      const char *buf, 
                                      size_t count)
{
    int new_value;
    int ret;
    
    ret = kstrtoint(buf, 10, &new_value);
    if (ret < 0) {
        pr_err("fastchg: invalid input '%s'\n", buf);
        return ret;
    }
    
    if (new_value == 0 || new_value == 1) {
        force_fast_charge = new_value;
        pr_debug("fastchg: set to %d\n", force_fast_charge);
    } else {
        pr_err("fastchg: value %d out of range (0-1)\n", new_value);
        return -EINVAL;
    }
    
    return count;
}

static struct kobj_attribute force_fast_charge_attribute = 
    __ATTR(force_fast_charge, 0664, force_fast_charge_show, force_fast_charge_store);

static struct attribute *force_fast_charge_attrs[] = {
    &force_fast_charge_attribute.attr,
    NULL,
};

static struct attribute_group force_fast_charge_attr_group = {
    .attrs = force_fast_charge_attrs,
};

static struct kobject *force_fast_charge_kobj;

static int __init force_fast_charge_init(void)
{
    int ret;
    
    force_fast_charge_kobj = kobject_create_and_add("fast_charge", kernel_kobj);
    if (!force_fast_charge_kobj) {
        pr_err("fastchg: failed to create kobject\n");
        return -ENOMEM;
    }
    
    ret = sysfs_create_group(force_fast_charge_kobj, &force_fast_charge_attr_group);
    if (ret) {
        pr_err("fastchg: failed to create sysfs group: %d\n", ret);
        kobject_put(force_fast_charge_kobj);
        return ret;
    }
    
    pr_debug("fastchg: initialized with value %d\n", force_fast_charge);
    return 0;
}

static void __exit force_fast_charge_exit(void)
{
    if (force_fast_charge_kobj) {
        sysfs_remove_group(force_fast_charge_kobj, &force_fast_charge_attr_group);
        kobject_put(force_fast_charge_kobj);
    }
    pr_debug("fastchg: module unloaded\n");
}

module_init(force_fast_charge_init);
module_exit(force_fast_charge_exit);
