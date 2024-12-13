# Something New Kernel

A customized kernel designed to optimize performance and features for Exynos 9611 devices.

## Features
- Removal of Samsung-specific security and debugging features.
- Based on Tab S6 Lite kernel source
- Compiled with Neutron Clang 18 and LLVM binutils.
- Includes Erofs, Incremental FS, BinderFS, and various backports.
- Enhanced DeX touchpad support for compatible OneUI versions.
- Strips away numerous Samsung-introduced debug codes/configurations.
- Integrates the [WireGuard](https://www.wireguard.com/) in-kernel VPN.

## Compilation Guide

**Prerequisites:** Linux based OS, at least 8GB RAM, and additional utilities.

```bash
# Install necessary packages.
sudo apt install -y git make libssl-dev curl bc pkg-config m4 libtool automake autoconf python3-is-python3

# Clone the repository.
git clone https://github.com/Exynos9611Development/android_kernel_samsung_universal9611

# Navigate to the repository.
cd android_kernel_samsung_universal9611

# Set up the toolchain
# Test various Clang/LLVM toolchains; Neutron Clang is preferred.
# For Arch or newer glibc distributions, consider using Antman.
bash <(curl -s https://gist.githubusercontent.com/cat658011/9462b1778231226b4fae0171a8cf1fd3/raw/setup-toolchain.sh)

# Start kernel build
# Additional flags:
# --oneui - Build for OneUI (Default AOSP variant).
# --allow-dirty - Build kernel without wiping out/ directory.
# --permissive - Build kernel with permissive SELinux policy.
python3 build_kernel.py --target=(select here your device, it can be a a51, f41, m31s, gta4xl, gta4xlwifi, m21) (Additional flags)
```
**Note:** The compilation process may take a significant amount of time and resources. Be prepared to wait some time for it to finish building.

The resulting ZIP file with the kernel will appear in the main repository directory after completion.

### Flashing Instructions

After a successful build, locate the SN_x.x.x_device_ROM_XXXX-XX-XX.zip file.
Flash the kernel using custom recovery or via adb sideload.

### Acknowledgments

- [cat658011](https://github.com/cat658011)
- [Tim Zammerman](https://github.com/linux4)
- [Royna2544](https://github.com/Royna2544)
- [Samsung Open Source](https://opensource.samsung.com/)
- [Android Open Source Project](https://source.android.com/)
- [The Linux Kernel](https://www.kernel.org/)

