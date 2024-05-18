# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers

## AnyKernel setup
# begin properties
properties() { '
kernel.string=
do.devicecheck=0
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=
device.name2=
device.name3=
device.name4=
device.name5=
supported.versions=
supported.patchlevels=
'; } # end properties

# shell variables
if [ -e /dev/block/platform/13520000.ufs/by-name/BOOT ]; then
	block=/dev/block/platform/13520000.ufs/by-name/BOOT;
elif [ -e /dev/block/platform/13520000.ufs/by-name/boot ]; then
	block=/dev/block/platform/13520000.ufs/by-name/boot;
fi

is_slot_device=0;
ramdisk_compression=auto;

## AnyKernel methods (DO NOT CHANGE)
# import patching functions/variables - see for reference
. tools/ak3-core.sh;


## AnyKernel file attributes
# set permissions/ownership for included ramdisk files
set_perm_recursive 0 0 755 644 $ramdisk/*;
set_perm_recursive 0 0 750 750 $ramdisk/init* $ramdisk/sbin;

# mount stuff
AK_FOLDER=/tmp/anykernel
mount /system/
mount /system_root/
#mount /vendor/
mount -o rw,remount -t auto /system > /dev/null
#mount -o rw,remount -t auto /vendor > /dev/null

cp $AK_FOLDER/init.grass.rc /system_root/system/etc/init/
chmod 644 system_root/system/etc/init/init.grass.rc


## AnyKernel install
#Method 1:
#dump_boot;
#write_boot;

#Method 2:
split_boot;
ui_print "- Installing Grass Kernel";
flash_boot;

ui_print "- Installation finished successfully";
ui_print " ";

#ui_print "- Thank you for using Eureka Kernel :)";
#ui_print " ";

## end install

