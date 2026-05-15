Import("env")
import shutil
import os

def rename_firmware(source, target, env):
    version = env.GetProjectOption("joystick_version")
    build_dir = env.subst("$BUILD_DIR")
    src = os.path.join(build_dir, "firmware.hex")
    dst = os.path.join(build_dir, "firmware_v{}.hex".format(version))
    if os.path.exists(src):
        shutil.copy2(src, dst)

env.AddPostAction("$BUILD_DIR/firmware.hex", rename_firmware)
