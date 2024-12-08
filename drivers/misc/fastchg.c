/*
 * Author: Chad Froebel <chadfroebel@gmail.com>
 *
 * Port to guacamole: engstk <eng.stk@sapo.pt>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Possible values for "force_fast_charge" are :
 *
 *   0 - Disabled (default)
 *   1 - Force faster charge
*/

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/fastchg.h>
#include <linux/string.h>
#include <linux/module.h>

int force_fast_charge = 1;

/**
 * get_fastcharge_opt - Set the force_fast_charge option based on input string.
 * @ffc: Input string representing the desired fast charge option ("0" or "1").
 *
 * This function parses the input string to set the global variable
 * force_fast_charge. If the input string is "0", fast charging is disabled.
 * If the input string is "1", fast charging is enabled. For any other input,
 * fast charging is enabled by default.
 *
 * Returns 1 to indicate the option has been processed.
 */
static int __init get_fastcharge_opt(char *ffc)
{
	if (strcmp(ffc, "0") == 0) {
		force_fast_charge = 0;
	} else if (strcmp(ffc, "1") == 0) {
		force_fast_charge = 1;
	} else {
		force_fast_charge = 0;
	}
	return 1;
}

__setup("ffc=", get_fastcharge_opt);

/**
 * force_fast_charge_show - Read the current fast charge option.
 * @kobj: kobject that contains the fast charge option.
 * @attr: kobj_attribute structure that describes the attribute.
 * @buf: buffer to store the current value of the fast charge option.
 *
 * This function reads the current value of the fast charge option and
 * stores it in the provided buffer.
 *
 * Returns the number of bytes written to the buffer.
 */
static ssize_t force_fast_charge_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	size_t count = 0;
	count += sprintf(buf, "%d\n", force_fast_charge);
	return count;
}


/**
 * force_fast_charge_store - Write the new fast charge option.
 * @kobj: kobject that contains the fast charge option.
 * @attr: kobj_attribute structure that describes the attribute.
 * @buf: buffer containing the new value for the fast charge option.
 * @count: size of the buffer.
 *
 * This function parses the input buffer to set the global variable
 * force_fast_charge. If the input is "0" or "1", it updates the variable
 * accordingly. For any other input, fast charging is disabled by default.
 *
 * Returns the number of bytes processed from the buffer.
 */
static ssize_t force_fast_charge_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d ", &force_fast_charge);
	if (force_fast_charge < 0 || force_fast_charge > 1)
		force_fast_charge = 0;

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

/* Initialize fast charge sysfs folder */
static struct kobject *force_fast_charge_kobj;

/**
 * force_fast_charge_init - Initialize fast charge sysfs folder.
 *
 * This function creates the "fast_charge" sysfs folder and adds the
 * force_fast_charge attribute to it.
 *
 * Returns 0 on success, -ENOMEM if the kobject creation fails.
 */
int force_fast_charge_init(void)
{
	int force_fast_charge_retval;

	force_fast_charge_kobj = kobject_create_and_add("fast_charge", kernel_kobj);
	if (!force_fast_charge_kobj) {
		return -ENOMEM;
	}

	force_fast_charge_retval = sysfs_create_group(force_fast_charge_kobj, &force_fast_charge_attr_group);

	if (force_fast_charge_retval)
		kobject_put(force_fast_charge_kobj);

	if (force_fast_charge_retval)
		kobject_put(force_fast_charge_kobj);

	return (force_fast_charge_retval);
}

/**
 * force_fast_charge_exit - Clean up the fast charge sysfs folder.
 *
 * This function is called when the module is unloaded. It removes the
 * "fast_charge" sysfs folder and all associated attributes.
 */
void force_fast_charge_exit(void)
{
	kobject_put(force_fast_charge_kobj);
}

module_init(force_fast_charge_init);
module_exit(force_fast_charge_exit);
