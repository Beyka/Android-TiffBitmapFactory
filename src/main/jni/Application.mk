APP_STL := stlport_static

APP_MODULES      := tiff tifffactory tiffsaver

APP_PLATFORM=android-8

#Do not build x32 and x64 modules in same time - it leads to errors in binaries
APP_ABI := armeabi-v7a x86 armeabi mips
#arm64-v8a x86_64 mips64
