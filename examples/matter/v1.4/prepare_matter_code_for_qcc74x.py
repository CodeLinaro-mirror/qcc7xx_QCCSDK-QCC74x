#!/usr/bin/env python3
import argparse
import os
import subprocess
import shutil
import stat
import sys
import platform

def check_dependencies():
    # List of required binaries (not python packages)
    required_bins = [
        'git', 'gcc', 'g++', 'pkg-config', 'cmake', 'openssl', 'dbus-daemon', 'glib-compile-schemas',
        'avahi-daemon', 'ninja', 'python3', 'python3-venv', 'python3-dev', 'python3-pip', 'unzip',
        'g-ir-compiler', 'cairo-trace', 'readline', 'java'
    ]
    missing = []
    for bin in required_bins:
        if shutil.which(bin) is None:
            missing.append(bin)
    if missing:
        print("\nMissing required system packages:")
        print("  " + " ".join(missing))
        print("\nPlease install them with:")
        print("sudo apt-get install git gcc g++ pkg-config cmake libssl-dev libdbus-1-dev libglib2.0-dev libavahi-client-dev ninja-build python3-venv python3-dev python3-pip unzip libgirepository1.0-dev libcairo2-dev libreadline-dev default-jre")
        print("\nThen run:")
        print("python3 prepare_matter_code_for_qcc74x.py <app_type>")
        sys.exit(1)

def run_command(command, cwd=None, env=None):
    print(f"Running: {command}")
    try:
        subprocess.run(command, shell=True, check=True, cwd=cwd, env=env)
    except subprocess.CalledProcessError as e:
        print(f"Error: Command failed with exit code {e.returncode}")
        print(f"Command: {e.cmd}")
        sys.exit(e.returncode)

def ensure_executable(script_path):
    if os.path.exists(script_path) and not os.access(script_path, os.X_OK):
        print(f"Making {script_path} executable")
        st = os.stat(script_path)
        os.chmod(script_path, st.st_mode | stat.S_IEXEC)

def main():
    # Warn if running on Windows without WSL
    if platform.system() == "Windows":
        print("Warning: This script is intended for Unix-like environments. Use WSL or a Linux shell.")

#    check_dependencies()

    parser = argparse.ArgumentParser(description="Setup Matter SDK environment.", add_help=False)
    parser.add_argument("app_type", type=int, choices=[1, 2], help="App type: 1 for contact sensor, 2 for light")
    # Print help if no arguments are given
    if len(sys.argv) == 1:
        parser.print_help(sys.stderr)
        print("\nExample usage:")
        print("python3 prepare_matter_code_for_qcc74x.py <app_type>")
        sys.exit(1)
    args = parser.parse_args()

    matter_dir = os.getcwd()
    # Set sdk_root to two levels up from the script's directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sdk_root = os.path.abspath(os.path.join(script_dir, "../../../"))
    matter_dir = os.path.abspath(os.path.join(script_dir, "../../../../"))

    os.makedirs(matter_dir, exist_ok=True)
    os.chdir(matter_dir)

    chip_dir = os.path.join(matter_dir, "connectedhomeip")
    if os.path.exists(chip_dir):
        print(f"{chip_dir} already exists. Skipping git clone.")
    else:
        run_command("git clone -b v1.4-branch https://github.com/project-chip/connectedhomeip.git")
    os.chdir(chip_dir)

    run_command("git checkout b5c706d27947e8487ad4a5128d43569262a23c45")
    run_command("./scripts/checkout_submodules.py --shallow --recursive --platform linux")

    # Copy patch files
    patch_src = os.path.join(sdk_root, "examples/matter/v1.4")
    for item in os.listdir(patch_src):
        if item.startswith("00"):
            src_file = os.path.join(patch_src, item)
            dst_file = os.path.join(chip_dir, item)
            try:
                shutil.copy(src_file, dst_file)
                print(f"Copied patch: {item}")
            except Exception as e:
                print(f"Failed to copy {item}: {e}")
                sys.exit(1)

    run_command("git am 00*")

    # Prepare environment variables for subprocesses
    env = os.environ.copy()
    env["QCC74X_SDK_ROOT"] = sdk_root
    env["QCC74X_SDK_TOOLCHAIN"] = os.path.join(sdk_root, "toolchain/linux_x86_64/bin")

    # Ensure scripts are executable
    ensure_executable("scripts/activate.sh")
    ensure_executable("scripts/bootstrap.sh")

    print("Environment setup complete.")

    # Run setup commands in bash
    setup_script = (
        "source scripts/activate.sh && "
        "scripts/bootstrap.sh -p qualcomm"
    )
    run_command(f"bash -c \"{setup_script}\"", env=env)

    # Determine the build command based on app_type
    if args.app_type == 1:
        build_command = "./scripts/build/build_examples.py --target qcc74x-qcc743dk-contact-sensor-wifi build"
    else:
        build_command = "./scripts/build/build_examples.py --target qcc74x-qcc743dk-light-wifi build"

    run_command(build_command, env=env)

if __name__ == "__main__":
    main()

