Import("env")
import os

# This PlatformIO hook applies only to the host-side native Unity test environment.
# Prefer PlatformIO's bundled MinGW compiler over a system-wide installation.
gcc_package = env.PioPlatform().get_package_dir("toolchain-gccmingw32")
gcc_bin = os.path.join(gcc_package, "bin") if gcc_package else None
if gcc_bin and os.path.exists(gcc_bin):
    env.PrependENVPath("PATH", gcc_bin)
    os.environ["PATH"] = gcc_bin + os.pathsep + os.environ.get("PATH", "")

# Link the MinGW C/C++ runtime statically so test executables need no external DLLs.
env.Append(LINKFLAGS=["-static", "-static-libgcc", "-static-libstdc++"])
