import argparse
import subprocess
import os
import shutil
import re

debug_popen_impl = False

def popen_impl(command: list[str]):
    if debug_popen_impl:
        print('Execute command: "%s"...' % ' '.join(command), end=' ')
    s = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = s.communicate()
    if s.returncode != 0:
        if debug_popen_impl:
            print('failed')
        out = out.decode("utf-8")
        err = err.decode("utf-8")
        stdout_log = str(s.pid) + "_stdout.log"
        stderr_log = str(s.pid) + "_stderr.log"
        with open(stdout_log, "w") as f:
            f.write(out)
        with open(stderr_log, "w") as f:
            f.write(err)
        print(f"[err] Check output log files: {stdout_log}, {stderr_log}")
        print(f"[err] Log files written to directory: {os.getcwd()}")
        raise RuntimeError(f"Command failed: {command}. Exitcode: {s.returncode}")
    if debug_popen_impl:
        print(f'result: {s.returncode == 0}')

def check_file(filename):
    print(f"Checking file if exists: {filename}...", end=' ')
    if not os.path.exists(filename):
        print("Not found")
    else:
        print("Found")
    return os.path.exists(filename)

def main():
    parser = argparse.ArgumentParser(description="Build Grass Kernel with specified arguments")
    
    parser.add_argument('--oneui', action='store_true', help="OneUI variant")
    parser.add_argument('--aosp', action='store_true', help="AOSP variant")
    parser.add_argument('--target', type=str, required=True, help="Target device (a51/m21/...)")
    parser.add_argument('--no-ksu', action='store_true', help="Don't include KernelSU support in kernel")

    # Parse the arguments
    args = parser.parse_args()
    
    if not args.oneui and not args.aosp:
        print("Please specify one of the following variants: --oneui or --aosp")
        return
    
    if not args.target in ['a51', 'f41', 'm31s', 'm31', 'm21']:
        print("Please specify a valid target: a51/f41/m31s/m31/m21")
        return
    
    # Check files
    if not check_file("scripts/packaging/pack.sh"):
        popen_impl(['git', 'submodule', 'update', '--init'])
    if not check_file("toolchain"):
        print(f"Please make toolchain available at {os.getcwd()}")
        return
    try:
        popen_impl(['./toolchain/bin/clang', '-v'])
    except RuntimeError as e:
        print("Failed to execute clang, something went wrong")
        raise e
    
    clangversionRegex = r"(.*?clang version \d+(\.\d+)*).*"
    s = subprocess.Popen(['./toolchain/bin/clang', '-v'], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    _, tcversion = s.communicate()
    tcversion = tcversion.decode('utf-8')
    matched = re.search(clangversionRegex, tcversion)
    if not matched:
        raise AssertionError('Failed to match: for version string: %s' % tcversion)
    tcversion = matched.group(1)
    
    # Print info
    print('================================')
    print('TARGET_KERNEL=Grass')
    print("TARGET_VARIANT=", end='')
    if args.oneui:
        print("OneUI")
    if args.aosp:
        print("AOSP")
    print(f'TARGET_DEVICE={args.target}')
    print(f'TARGET_INCLUDES_KSU={not args.no_ksu}')
    print('TARGET_USES_LLVM=True')
    print(f'TOOLCHAIN={tcversion}')
    print('================================')
    
    # Add toolchain in PATH environment variable
    tcPath = os.path.join(os.getcwd(), 'toolchain', 'bin')
    if tcPath not in os.environ['PATH'].split(os.pathsep):
        os.environ["PATH"] += ':' + tcPath
    
    if os.path.exists('out'):
        print('Make clean...')
        shutil.rmtree('out')
    
    make_common = ['make', 'O=out', 'LLVM=1', f'-j{os.cpu_count()}']
    make_defconfig = make_common + [f'vendor/{args.target}_defconfig',
                                    'vendor/grass.config', f'vendor/{args.target}.config']
    if not args.no_ksu:
        make_defconfig.append('vendor/ksu.config')
    if args.aosp:
        make_defconfig.append('vendor/aosp.config')
    
    print('Make defconfig...')
    popen_impl(make_defconfig)
    print('Make kernel...')
    popen_impl(make_common)
    print('Done')
    
if __name__ == '__main__':
    main()