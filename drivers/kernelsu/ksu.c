#include <linux/export.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <generated/utsrelease.h>
#include <generated/compile.h>
#include <linux/version.h> /* LINUX_VERSION_CODE, KERNEL_VERSION macros */
#include <linux/workqueue.h>

#include "allowlist.h"
#include "core_hook.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "throne_tracker.h"

#ifdef CONFIG_KSU_SUSFS
#include <linux/susfs.h>
#endif

static struct workqueue_struct *ksu_workqueue;

bool ksu_queue_work(struct work_struct *work)
{
	return queue_work(ksu_workqueue, work);
}

#ifdef KSU_USE_STRUCT_FILENAME
extern int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
					void *argv, void *envp, int *flags);

extern int ksu_handle_execveat_ksud(int *fd, struct filename **filename_ptr,
				    void *argv, void *envp, int *flags);

int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,
			void *envp, int *flags)
{
	ksu_handle_execveat_ksud(fd, filename_ptr, argv, envp, flags);
	return ksu_handle_execveat_sucompat(fd, filename_ptr, argv, envp,
					    flags);
}
#endif // KSU_USE_STRUCT_FILENAME

// track backports and other quirks here
// ref: kernel_compat.c, Makefile
// yes looks nasty
#ifdef KSU_USE_STRUCT_FILENAME
	#define FEAT_1 " +uses_struct_filename"
#else
	#define FEAT_1 ""
#endif
#ifndef CONFIG_KSU_LSM_SECURITY_HOOKS
	#define FEAT_2 " -lsm_hooks"
#else
	#define FEAT_2 ""
#endif
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)) && defined(CONFIG_KSU_ALLOWLIST_WORKAROUND)
	#define FEAT_3 " +allowlist_workaround"
#else
	#define FEAT_3 ""
#endif
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)) && defined(KSU_HAS_MODERN_EXT4)
	#define FEAT_4 " +ext4_unregister_sysfs"
#else
	#define FEAT_4 ""
#endif
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)) && defined(KSU_HAS_GET_CRED_RCU)
	#define FEAT_5 " +get_cred_rcu"
#else
	#define FEAT_5 ""
#endif
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)) && defined(KSU_HAS_PATH_UMOUNT)
	#define FEAT_6 " +path_umount"
#else
	#define FEAT_6 ""
#endif
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)) && defined(KSU_STRNCPY_FROM_USER_NOFAULT)
	#define FEAT_7 " +strncpy_from_user_nofault"
#else
	#define FEAT_7 ""
#endif
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)) && defined(KSU_STRNCPY_FROM_UNSAFE_USER)
	#define FEAT_8 " +strncpy_from_unsafe_user"
#else
	#define FEAT_8 ""
#endif
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)) && defined(KSU_NEW_KERNEL_READ)
	#define FEAT_9 " +new_kernel_read"
#else
	#define FEAT_9 ""
#endif
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)) && defined(KSU_NEW_KERNEL_WRITE)
	#define FEAT_10 " +new_kernel_write"
#else
	#define FEAT_10 ""
#endif

#define EXTRA_FEATURES FEAT_1 FEAT_2 FEAT_3 FEAT_4 FEAT_5 FEAT_6 FEAT_7 FEAT_8 FEAT_9 FEAT_10

int __init ksu_kernelsu_init(void)
{
	pr_info("Initialized on: %s (%s) with ksuver: %s%s\n", UTS_RELEASE, UTS_MACHINE, __stringify(KSU_VERSION), EXTRA_FEATURES);

#ifdef CONFIG_KSU_DEBUG
	pr_alert("*************************************************************");
	pr_alert("**     NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE    **");
	pr_alert("**                                                         **");
	pr_alert("**         You are running KernelSU in DEBUG mode          **");
	pr_alert("**                                                         **");
	pr_alert("**     NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE    **");
	pr_alert("*************************************************************");
#endif

#ifdef CONFIG_KSU_SUSFS
	susfs_init();
#endif

	ksu_core_init();

	ksu_workqueue = alloc_ordered_workqueue("kernelsu_work_queue", 0);

	ksu_allowlist_init();

	ksu_throne_tracker_init();

	return 0;
}

void ksu_kernelsu_exit(void)
{
	ksu_allowlist_exit();

	ksu_throne_tracker_exit();

	destroy_workqueue(ksu_workqueue);

}

module_init(ksu_kernelsu_init);
module_exit(ksu_kernelsu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weishu");
MODULE_DESCRIPTION("Android KernelSU");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
