"""Explicit native 12340 x86 toolchain selection shared by ALL DLL recipes.

GitHub-hosted windows-2022 images change independently; default "latest" MSVC
and SDK can generate different DLL bytes for identical sources. The v142
compatibility toolset and SDK 19041 are installed on both previously observed
runner images. If this exact pair becomes unavailable, fail closed and migrate
it deliberately rather than silently compiling with a different "latest".
"""
PINNED_VC_VERSION = "14.29"
PINNED_WINDOWS_SDK = "10.0.19041.0"
VCVARS_ARGS = ("x86 " + PINNED_WINDOWS_SDK +
               " -vcvars_ver=" + PINNED_VC_VERSION)
