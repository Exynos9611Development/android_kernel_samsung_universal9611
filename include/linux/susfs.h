#ifndef KSU_SUSFS_H
#define KSU_SUSFS_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/hashtable.h>
#include <linux/path.h>
#include <linux/susfs_def.h>

#define SUSFS_VERSION "v1.5.5"
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#define SUSFS_VARIANT "NON-GKI"
#else
#define SUSFS_VARIANT "GKI"
#endif

/*********/
/* MACRO */
/*********/
#define getname_safe(name) (name == NULL ? ERR_PTR(-EINVAL) : getname(name))
#define putname_safe(name) (IS_ERR(name) ? NULL : putname(name))

/**********/
/* STRUCT */
/**********/
/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
struct st_susfs_sus_path {
	unsigned long                    target_ino;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
};

struct st_susfs_sus_path_hlist {
	unsigned long                    target_ino;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	struct hlist_node                node;
};
#endif

/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
struct st_susfs_sus_mount {
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long           target_dev;
};

struct st_susfs_sus_mount_list {
	struct list_head                        list;
	struct st_susfs_sus_mount               info;
};
#endif

/* try_umount */
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
struct st_susfs_try_umount {
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                     mnt_mode;
};

struct st_susfs_try_umount_list {
	struct list_head                        list;
	struct st_susfs_try_umount              info;
};
#endif

/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
struct st_susfs_uname {
	char        release[__NEW_UTS_LEN+1];
	char        version[__NEW_UTS_LEN+1];
};
#endif

/***********************/
/* FORWARD DECLARATION */
/***********************/
/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
int susfs_add_sus_path(struct st_susfs_sus_path* __user user_info);
int susfs_sus_ino_for_filldir64(unsigned long ino);
#endif
/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
int susfs_add_sus_mount(struct st_susfs_sus_mount* __user user_info);
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT

/* try_umount */
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
int susfs_add_try_umount(struct st_susfs_try_umount* __user user_info);
void susfs_try_umount(uid_t target_uid);
#endif // #ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
int susfs_set_uname(struct st_susfs_uname* __user user_info);
void susfs_spoof_uname(struct new_utsname* tmp);
#endif
/* set_log */
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_set_log(bool enabled);
#endif
/* susfs_init */
void susfs_init(void);

#endif
