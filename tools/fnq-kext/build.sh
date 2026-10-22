#!/bin/bash
# Збірка FnQNotify.kext Command Line Tools'ами (без Xcode) -> build/FnQNotify.kext (ad-hoc підпис)
set -euo pipefail
cd "$(dirname "$0")"
SDK=$(xcrun --sdk macosx --show-sdk-path); KHDR="$SDK/System/Library/Frameworks/Kernel.framework/Headers"
rm -rf build; mkdir -p build/FnQNotify.kext/Contents/MacOS
xcrun clang++ -arch x86_64 -mmacosx-version-min=15.0 -fapple-kext -mkernel -nostdinc -std=c++17 -O2 -Wall \
  -I"$KHDR" -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -c FnQNotify.cpp -o build/FnQNotify.o
cat > build/info.c <<'C'
#include <mach/mach_types.h>
extern kern_return_t _start(kmod_info_t *, void *);
extern kern_return_t _stop(kmod_info_t *, void *);
KMOD_EXPLICIT_DECL(local.belik.FnQNotify, "1.0", _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
C
xcrun clang -arch x86_64 -mmacosx-version-min=15.0 -fapple-kext -mkernel -nostdinc -I"$KHDR" -DKERNEL -c build/info.c -o build/info.o
xcrun clang++ -arch x86_64 -mmacosx-version-min=15.0 -fapple-kext -nostdlib -Xlinker -kext -lkmodc++ -lkmod -lcc_kext \
  build/FnQNotify.o build/info.o -o build/FnQNotify.kext/Contents/MacOS/FnQNotify
cp Info.plist build/FnQNotify.kext/Contents/Info.plist
codesign -s - -f --digest-algorithm=sha256 build/FnQNotify.kext
shasum -a 256 build/FnQNotify.kext/Contents/MacOS/FnQNotify
