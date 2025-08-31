import argparse
import subprocess
import os
import shutil
import re
from datetime import datetime
import sys
import zipfile
class CommandError(Exception):
    pass

def run_command(command):
    try:
        run = subprocess.run(command,
                             capture_output=True,
                             text=True,
                             check=True,
                             cwd=None)
        return run.stdout, run.stderr
    except subprocess.CalledProcessError as e:
        raise CommandError(f"Command failed: {e.cmd}\nError: {e.stderr}") from e

def file_exists(filepath):
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Required file not found: {filepath}")
    return True

def extract_match(regex, text):
    match = re.search(regex, text)
    if not match:
        raise AssertionError(f'Failed to match pattern: {pattern} with regex: {regex}')
    return match.group(1)

def display_info(info_dict):
    print('================================')
    for key, value in info_dict.items():
        print(f"{key}={value}")
    print('================================')

def log(message):
    timestamp = datetime.now().strftime('%H:%M:%S')
    print(f"[{timestamp}] {message}")

def create_zip(zip_filename, files):
    log(f"Creating zip: {zip_filename} with {len(files)} files")
    with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for file in files:
            zf.write(file)
    log("Zip creation complete")

def copy_file(src, to):
    if not os.path.exists(src):
        raise FileNotFoundError(f"File does not exist: {src}")
    shutil.copyfile(src, to)

class ClangCompiler:
    @staticmethod
    def verify_executable():
        try:
            run_command(['./toolchain/bin/clang', '-v'])
        except CommandError:
            print("Clang execution failed")
            raise

    @staticmethod
    def get_version():
        version_regex = r"(.*?clang version \d+(\.\d+)*)"
        _, stderr_output = run_command(['./toolchain/bin/clang', '-v'])
        return extract_match(version_regex, stderr_output)

def main():
    parser = argparse.ArgumentParser(description="Build Something New Kernel with specified arguments")
    parser.add_argument('--target', type=str, required=True, help="Target device (a51/m21/...)", choices=['a51', 'f41', 'm31s', 'm31', 'm21', 'gta4xl', 'gta4xlwifi'])
    parser.add_argument('--ksu', action='store_true', help="Build KernelSU variant")
    parser.add_argument('--allow-dirty', action='store_true', help="Allow dirty build")
    args = parser.parse_args()

    if not file_exists("AnyKernel3/anykernel.sh"):
        run_command(['git', 'submodule', 'update', '--init'])
    if not file_exists("toolchain/bin/clang"):
        print(f"Toolchain must be available at {os.getcwd()}/toolchain")
        return

    ClangCompiler.verify_executable()

    parent_dir = os.getcwd()
    output_dir = f'{parent_dir}/out'
    current_branch = subprocess.run(['git', 'branch', '--show-current'],
                                     capture_output=True, text=True).stdout.strip()
    current_commit = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'],
                                     capture_output=True, text=True).stdout.strip()

    display_info({
        'Kernel name': 'Something New',
        'Branch': f'{current_branch}/{current_commit}',
        'Device': args.target,
        'KernelSU': args.ksu,
        'Compiler version': ClangCompiler.get_version(),
    })

    toolchain_path = f'{parent_dir}/toolchain/bin'
    if toolchain_path not in os.environ['PATH'].split(os.pathsep):
        os.environ["PATH"] = toolchain_path + ':' + os.environ["PATH"]

    if os.path.exists(output_dir) and not args.allow_dirty:
        log('Cleaning build output...')
        shutil.rmtree(output_dir)

    make_common = ['make', 'O=out', 'LLVM=1', 'CROSS_COMPILE=aarch64-linux-gnu-',
                   'CC=clang', 'LD=ld.lld', 'AS=llvm-as', 'AR=llvm-ar',
                   'OBJDUMP=llvm-objdump', 'READELF=llvm-readelf', 'NM=llvm-nm',
                   'OBJCOPY=llvm-objcopy', 'ARCH=arm64', f'-j{os.cpu_count()}']
    make_defconfig = make_common + [f'exynos9611-{args.target}_defconfig']
    if args.ksu:
        make_defconfig.append('ksu.config')

    start_time = datetime.now()
    log('Running make defconfig...')
    run_command(make_defconfig)
    log('Building kernel...')
    run_command(make_common)
    log('Building dtbo image')
    run_command(['python3', f'{parent_dir}/build_kernel/bin/mkdtboimg.py',
                 'cfg_create', f'{output_dir}/arch/arm64/boot/dtbo-{args.target}.img',
                 f'{parent_dir}/build_kernel/configs/dtbo/{args.target}.cfg',
                 '-d', f'{output_dir}/arch/arm64/boot/dts/samsung'])
    log('Building dtb image')
    run_command(['python3', 'build_kernel/bin/mkdtboimg.py',
                 'cfg_create', f'{output_dir}/arch/arm64/boot/exynos9611.dtb',
                 f'{parent_dir}/build_kernel/configs/dtb/exynos9611.cfg',
                 '--dtb-dir', f'{output_dir}/arch/arm64/boot/dts/exynos'])
    log('Build complete')
    elapsed_time = datetime.now() - start_time

    anykernel3_dir = f'{parent_dir}/AnyKernel3'

    with open(os.path.join(output_dir, 'include', 'generated', 'utsrelease.h')) as f:
        kernel_version_info = extract_match(r'"([^"]+)"', f.read())

    copy_file(f'{output_dir}/arch/arm64/boot/Image', f'{anykernel3_dir}/Image')
    copy_file(f'{output_dir}/arch/arm64/boot/dtbo-{args.target}.img', f'{anykernel3_dir}/dtbo.img')
    copy_file(f'{output_dir}/arch/arm64/boot/exynos9611.dtb', f'{anykernel3_dir}/dtb')
    ksu = 'KSU' if args.ksu else 'Non-KSU'
    zip_filename = 'SN_{}_{}_{}.zip'.format(
        args.target, datetime.today().strftime('%Y-%m-%d'), ksu)

    os.chdir(anykernel3_dir)
    create_zip(zip_filename, [
        'Image',
        'dtbo.img',
        'dtb',
        'META-INF/com/google/android/update-binary',
        'META-INF/com/google/android/updater-script',
        'tools/ak3-core.sh',
        'tools/busybox',
        'tools/magiskboot',
        'anykernel.sh',
        'version'
    ])
    final_zip_path = f'{parent_dir}/{zip_filename}'
    try:
        os.remove(f'{anykernel3_dir}/Image')
        os.remove(f'{anykernel3_dir}/dtbo.img')
        os.remove(f'{anykernel3_dir}/dtb')
        os.remove(final_zip_path)
    except FileNotFoundError:
        pass
    shutil.move(zip_filename, final_zip_path)
    os.chdir('..')
    display_info({
        'Zip name': zip_filename,
        'Kernel version': kernel_version_info,
        'Elapsed time': f"{elapsed_time.total_seconds()} seconds"
    })

if __name__ == '__main__':
    try:
        main()
    except (CommandError, FileNotFoundError, AssertionError, subprocess.CalledProcessError) as e:
        print(f"Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("Build interrupted by user")
        sys.exit(1)
