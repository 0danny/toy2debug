import sys
sys.dont_write_bytecode = True

import os
import subprocess
import argparse

# Constants
PROJECT_NAME = "toy2debug"
LAUNCH_NAME = "loader"
BUILD_FOLDER = "build"

def track_process(command, custom_name=""):
    proc = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )

    name = command[0] if custom_name == "" else custom_name

    for line in proc.stdout:
        print(f"[{name}]: {line.strip()}")

    proc.wait()
    if proc.returncode != 0:
        raise subprocess.CalledProcessError(proc.returncode, command)

def copy_assets():
    # Placeholder for asset copy logic
    print("[build]: Assets have been copied to build folder.")

def get_executable_name():
    return f"{LAUNCH_NAME}.exe" if os.name == 'nt' else f"./{LAUNCH_NAME}"

def run_build(launch_app, build_type):
    print(f"[build]: Running build for {PROJECT_NAME} with type {build_type}")

    # Prepare build directory
    os.makedirs(BUILD_FOLDER, exist_ok=True)
    os.chdir(BUILD_FOLDER)

    # Configure with CMake for 32-bit
    track_process([
        'cmake',
        '..',
        "-G", "Visual Studio 17 2022",
        "-A", "Win32",
    ])

    # Build with MSBuild
    track_process(['cmake', '--build', '.', '--config', build_type])

    # Copy any assets
    copy_assets()

    # Launch application if requested
    if launch_app:
        output_folder = os.path.join(os.getcwd(), build_type)
        exe_path = os.path.join(output_folder, get_executable_name())

        print(f"----------------- Starting {PROJECT_NAME} -----------------")
        subprocess.run([exe_path], cwd=output_folder)

def main():
    parser = argparse.ArgumentParser(
        description=f"Build script for {PROJECT_NAME}."
    )
    parser.add_argument("--nl", action="store_true", help="Don't launch the app after build")

    parser.add_argument(
        "--bt",
        choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
        default="RelWithDebInfo",
        help="CMake build type (default: RelWithDebInfo)"
    )

    args = parser.parse_args()

    run_build(launch_app=not args.nl, build_type=args.bt)

if __name__ == "__main__":
    main()
