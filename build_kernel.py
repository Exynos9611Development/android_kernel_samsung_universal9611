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
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=True,
            cwd=None
        )
        return result.stdout, result.stderr
    except subprocess.CalledProcessError as e:
        raise CommandError(f"Command failed: {e.cmd}\nError: {e.stderr}") from e

def file_exists(filepath):
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Required file not found: {filepath}")
    return True

def extract_match(regex, text):
    match = re.search(regex, text)
    if not match:
        raise AssertionError(f'Failed to match pattern with regex: {regex}')
    return match.group(1)

def display_info(info_dict):
    print('================================')
    for key, value in info_dict.items():
        print(f"{key}: {value}")
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

def copy_file(src, dest):
    if not os.path.exists(src):
        raise FileNotFoundError(f"File does not exist: {src}")
    shutil.copyfile(src, dest)

class ClangCompiler:
    CLANG_PATH = './toolchain/bin/clang'
    
    @staticmethod
    def verify_executable():
        try:
            run_command([ClangCompiler.CLANG_PATH, '-v'])
        except CommandError:
            print("Clang execution failed")
            raise

    @staticmethod
    def get_version():
        version_regex = r"(.*?clang version \d+(?:\.\d+)*)"
        _, stderr_output = run_command([ClangCompiler.CLANG_PATH, '-v'])
        return extract_match(version_regex, stderr_output)

def main():
    parser = argparse.ArgumentParser(
        description="Build Something New Kernel with specified arguments"
    )
    parser.add_argument(
        '--target',
        type=str,
        required=True,
        help="Target device (a51/m21/...)",
        choices=['a51', 'f41', 'm31s', 'm31', 'm21', 'gta4xl', 'gta4xlwifi']
    )
    parser.add_argument(
        '--allow-dirty',
        action='store_true',
        help="Allow dirty build"
    )
    parser.add_argument(
        '--ksu',
        action='store_true',
        help="Enable KernelSU"
    )
    parser.add_argument(
        '--oneui',
        action='store_true',
        help="Build OneUI variant"
    )
    args = parser.parse_args()

    if (not os.path.exists("AnyKernel3/anykernel.sh")) or os.path.exists("AnyKernel3/KernelSU-Next"):
        print(f"Updating git submodules")
        run_command(['git', 'submodule', 'update', '--init'])
    
    if not file_exists("toolchain/bin/clang"):
        print(f"Toolchain must be available at {os.getcwd()}/toolchain")
        return

    ClangCompiler.verify_executable()

    parent_dir = os.getcwd()
    output_dir = f'{parent_dir}/out'
    
    current_branch = subprocess.run(
        ['git', 'branch', '--show-current'],
        capture_output=True,
        text=True
    ).stdout.strip()
    
    current_commit = subprocess.run(
        ['git', 'rev-parse', '--short', 'HEAD'],
        capture_output=True,
        text=True
    ).stdout.strip()

    rom_tag = 'OUI' if args.oneui else 'AOSP'
    display_info({
        'Kernel name': 'Something New',
        'Branch': f'{current_branch}/{current_commit}',
        'Device': args.target,
        'ROM': rom_tag,
        'Compiler version': ClangCompiler.get_version(),
        'KSU': args.ksu,
    })

    toolchain_path = f'{parent_dir}/toolchain/bin'
    if toolchain_path not in os.environ['PATH'].split(os.pathsep):
        os.environ["PATH"] = f"{toolchain_path}:{os.environ['PATH']}"

    if os.path.exists(output_dir) and not args.allow_dirty:
        log('Cleaning build output...')
        shutil.rmtree(output_dir)

    make_common = [
        'make', 'O=out', 'LLVM=1', 'CROSS_COMPILE=aarch64-linux-gnu-',
        'CC=clang', 'LD=ld.lld', 'AS=llvm-as', 'AR=llvm-ar',
        'OBJDUMP=llvm-objdump', 'READELF=llvm-readelf', 'NM=llvm-nm',
        'OBJCOPY=llvm-objcopy', 'ARCH=arm64', f'-j{os.cpu_count()}'
    ]
    make_defconfig = make_common + [f'exynos9611-{args.target}_defconfig']
    if args.ksu:
        make_defconfig.append('ksu.config')
    if args.oneui:
        make_defconfig.append('oneui.config')

    start_time = datetime.now()
    
    log('Running make defconfig...')
    run_command(make_defconfig)
    log('Building kernel...')
    run_command(make_common)
    
    log('Building dtbo image')
    run_command([
        'python3', f'{parent_dir}/build_kernel/bin/mkdtboimg.py',
        'cfg_create', f'{output_dir}/arch/arm64/boot/dtbo-{args.target}.img',
        f'{parent_dir}/build_kernel/configs/dtbo/{args.target}.cfg',
        '-d', f'{output_dir}/arch/arm64/boot/dts/samsung'
    ])
    
    log('Building dtb image')
    run_command([
        'python3', 'build_kernel/bin/mkdtboimg.py',
        'cfg_create', f'{output_dir}/arch/arm64/boot/exynos9611.dtb',
        f'{parent_dir}/build_kernel/configs/dtb/exynos9611.cfg',
        '--dtb-dir', f'{output_dir}/arch/arm64/boot/dts/exynos'
    ])
    
    log('Build complete')
    elapsed_time = datetime.now() - start_time

    anykernel3_dir = f'{parent_dir}/AnyKernel3'
    utsrelease_path = os.path.join(output_dir, 'include', 'generated', 'utsrelease.h')
    
    with open(utsrelease_path) as f:
        kernel_version = extract_match(r'"([^"]+)"', f.read())
    
    copy_file(f'{output_dir}/arch/arm64/boot/Image', f'{anykernel3_dir}/Image')
    copy_file(f'{output_dir}/arch/arm64/boot/dtbo-{args.target}.img', f'{anykernel3_dir}/dtbo.img')
    copy_file(f'{output_dir}/arch/arm64/boot/exynos9611.dtb', f'{anykernel3_dir}/dtb')

    ksu_tag = '_KSU' if args.ksu else ''
    zip_filename = f'SN{ksu_tag}_{args.target}_{rom_tag}_{datetime.today().strftime("%Y-%m-%d")}.zip'

    os.chdir('AnyKernel3/')
    
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
    
    for file in ['Image', 'dtbo.img', 'dtb', final_zip_path]:
        try:
            os.remove(f'{anykernel3_dir}/{file}' if file != final_zip_path else file)
        except FileNotFoundError:
            pass
    
    shutil.move(zip_filename, final_zip_path)
    os.chdir('..')
    
    display_info({
        'Zip name': zip_filename,
        'Kernel version': kernel_version,
        'Elapsed time': f"{elapsed_time.total_seconds():.2f} seconds"
    })

if __name__ == '__main__':
    try:
        main()
    except (CommandError, FileNotFoundError, AssertionError, subprocess.CalledProcessError) as e:
        print(f"Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nBuild interrupted by user")
        sys.exit(1)
