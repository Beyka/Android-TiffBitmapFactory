APP_STL := c++_static

APP_MODULES      := tiff png jpeg tifffactory tiffsaver tiffconverter

APP_PLATFORM=android-21

APP_ABI := all

APP_SUPPORT_FLEXIBLE_PAGE_SIZES := true
APP_LDFLAGS += -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384

APP_OPTIM := debug
