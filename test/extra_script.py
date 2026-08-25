Import("env")
import os

# This PlatformIO hook applies only to the host-side native Unity test environment.
# Prefer PlatformIO's bundled MinGW compiler over a system-wide installation.
gcc_bin = os.path.abspath(
    os.path.join(
        os.path.expanduser("~"),
        ".platformio",
        "packages",
        "toolchain-gccmingw32",
        "bin",
    )
)
if os.path.exists(gcc_bin):
    env.PrependENVPath("PATH", gcc_bin)
    os.environ["PATH"] = gcc_bin + os.pathsep + os.environ.get("PATH", "")

# Link the MinGW C/C++ runtime statically so test executables need no external DLLs.
env.Append(LINKFLAGS=["-static", "-static-libgcc", "-static-libstdc++"])
