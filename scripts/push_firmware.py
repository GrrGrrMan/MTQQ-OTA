import hashlib
import os
import shutil
import subprocess

Import("env")  # noqa: F821


def push_firmware(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    bin_src = str(target[0])

    with open(bin_src, "rb") as f:
        new_md5 = hashlib.md5(f.read()).hexdigest()

    # Read existing MD5 if it exists
    md5_file = os.path.join(project_dir, "firmware.md5")
    old_md5 = ""
    if os.path.exists(md5_file):
        with open(md5_file, "r") as f:
            old_md5 = f.read().strip()

    if new_md5 == old_md5:
        print("\n[OTA] Firmware unchanged, skipping push.\n")
        return

    print(f"\n[OTA] New firmware! MD5: {new_md5}")
    shutil.copy(bin_src, os.path.join(project_dir, "firmware.bin"))
    with open(md5_file, "w") as f:
        f.write(new_md5)

    try:
        subprocess.run(
            ["git", "add", "firmware.bin", "firmware.md5"], cwd=project_dir, check=True
        )
        subprocess.run(
            ["git", "commit", "-m", f"OTA: {new_md5[:8]}"], cwd=project_dir, check=True
        )
        subprocess.run(["git", "push"], cwd=project_dir, check=True)
        print("[OTA] Pushed. ESP32 will update within 30s.\n")
    except subprocess.CalledProcessError as e:
        print(f"[OTA] Git push failed: {e}\n")


# Tell PlatformIO to run this after building the .bin
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", push_firmware)  # noqa: F821
