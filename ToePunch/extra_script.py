import os
import re
import shutil

Import("env")

# cesta k version.h
version_file = os.path.join(env["PROJECT_DIR"], "src", "version.h")

with open(version_file) as f:
    text = f.read()

version = re.search(r'#define\s+VERSION\s+"([^"]+)"', text).group(1)
build = re.search(r'#define\s+BUILD\s+(\d+)', text).group(1)

def after_build(source, target, env):
    firmware_path = target[0].get_abspath()
    dest_dir = os.path.join(env["PROJECT_DIR"], "builds", version)
    os.makedirs(dest_dir, exist_ok=True)
    dest_file = os.path.join(dest_dir, f"{build}.bin")
    shutil.copy(firmware_path, dest_file)
    version_file = os.path.join(dest_dir, "firmware.version")
    with open(version_file, 'w', encoding='utf-8') as f:
        f.write(f"{build}\n")
    print(f"Firmware copied to: {dest_file}")

env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)
