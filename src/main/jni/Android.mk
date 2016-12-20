LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_JPEG_SRC_FILES := \
	jpeg8d/jcapimin.c jpeg8d/jcapistd.c jpeg8d/jccoefct.c jpeg8d/jccolor.c jpeg8d/jcdctmgr.c jpeg8d/jchuff.c \
	jpeg8d/jcinit.c jpeg8d/jcmainct.c jpeg8d/jcmarker.c jpeg8d/jcmaster.c jpeg8d/jcomapi.c jpeg8d/jcparam.c \
	jpeg8d/jcprepct.c jpeg8d/jcsample.c jpeg8d/jctrans.c jpeg8d/jdapimin.c jpeg8d/jdapistd.c \
	jpeg8d/jdatadst.c jpeg8d/jdatasrc.c jpeg8d/jdcoefct.c jpeg8d/jdcolor.c jpeg8d/jddctmgr.c jpeg8d/jdhuff.c \
	jpeg8d/jdinput.c jpeg8d/jdmainct.c jpeg8d/jdmarker.c jpeg8d/jdmaster.c jpeg8d/jdmerge.c \
	jpeg8d/jdpostct.c jpeg8d/jdsample.c jpeg8d/jdtrans.c jpeg8d/jerror.c jpeg8d/jfdctflt.c jpeg8d/jfdctfst.c \
	jpeg8d/jfdctint.c jpeg8d/jidctflt.c jpeg8d/jidctfst.c jpeg8d/jidctint.c jpeg8d/jquant1.c \
	jpeg8d/jquant2.c jpeg8d/jutils.c jpeg8d/jmemmgr.c jpeg8d/jcarith.c jpeg8d/jdarith.c jpeg8d/jaricom.c

LOCAL_TIFF_SRC_FILES := \
	tiff/libtiff/tif_dirread.c \
	tiff/libtiff/tif_zip.c \
	tiff/libtiff/tif_flush.c \
	tiff/libtiff/tif_next.c \
	tiff/libtiff/tif_ojpeg.c \
	tiff/libtiff/tif_dirwrite.c \
	tiff/libtiff/tif_dirinfo.c \
	tiff/libtiff/tif_dir.c \
	tiff/libtiff/tif_compress.c \
	tiff/libtiff/tif_close.c \
	tiff/libtiff/tif_tile.c \
	tiff/libtiff/tif_open.c \
	tiff/libtiff/tif_getimage.c \
	tiff/libtiff/tif_pixarlog.c \
	tiff/libtiff/tif_warning.c \
	tiff/libtiff/tif_dumpmode.c \
	tiff/libtiff/tif_jpeg.c \
	tiff/libtiff/tif_jbig.c \
	tiff/libtiff/tif_predict.c \
	tiff/libtiff/mkg3states.c \
	tiff/libtiff/tif_write.c \
	tiff/libtiff/tif_error.c \
	tiff/libtiff/tif_version.c \
	tiff/libtiff/tif_print.c \
	tiff/libtiff/tif_color.c \
	tiff/libtiff/tif_read.c \
	tiff/libtiff/tif_extension.c \
	tiff/libtiff/tif_thunder.c \
	tiff/libtiff/tif_lzw.c \
	tiff/libtiff/tif_fax3.c \
	tiff/libtiff/tif_luv.c \
	tiff/libtiff/tif_codec.c \
	tiff/libtiff/tif_unix.c \
	tiff/libtiff/tif_packbits.c \
	tiff/libtiff/tif_aux.c \
	tiff/libtiff/tif_fax3sm.c \
	tiff/libtiff/tif_swab.c \
	tiff/libtiff/tif_strip.c

LOCAL_TIFF_SRC_FILES += tiff/port/lfind.c 
#######################LIBJPEG8D#################################

#include $(CLEAR_WARS)
#LOCAL_MODULE := cocos_jpeg_static
#LOCAL_MODULE_FILENAME := libjpeg
#LOCAL_SRC_FILES:= $(LOCAL_JPEG_SRC_FILES)
#LOCAL_SRC_FILES += \
#	jpeg8d/jmemnobs.c
#LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/jpeg8d
#include $(BUILD_STATIC_LIBRARY)



#include $(CLEAR_VARS)

#LOCAL_MODULE := libjpeg
#LOCAL_EXPORT_C_INCLUDES := $(GENERATED_INCLUDE_PATH)/assimp/include
#LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libjpeg.a

#include $(PREBUILT_STATIC_LIBRARY)

###########################################################
LOCAL_SRC_FILES:= $(LOCAL_TIFF_SRC_FILES)
LOCAL_C_INCLUDES += \
					$(LOCAL_PATH)/tiff/libtiff \
					$(LOCAL_PATH)/jpeg


#LOCAL_STATIC_LIBRARIES := \
#					$(LOCAL_PATH)/libs/libjpeg.a \
#					$(LOCAL_PATH)/libs/libjpeg-x86.a

LOCAL_CFLAGS += -DAVOID_TABLES
LOCAL_CFLAGS += -O3 -fstrict-aliasing -fprefetch-loop-arrays
LOCAL_MODULE:= libtiff
LOCAL_LDLIBS := -lz
	#-L $(LOCAL_PATH)/libs \
	#-L $(LOCAL_STATIC_LIBRARIES) \
	#-ljpeg

LOCAL_LDLIBS += $(LOCAL_PATH)/libs/$(TARGET_ARCH_ABI)/libjpeg.a

#LOCAL_PRELINK_MODULE:=false
include $(BUILD_SHARED_LIBRARY)

###############################################################
include $(CLEAR_VARS)
LOCAL_MODULE := tifffactory
LOCAL_CFLAGS := -DANDROID_NDK
LOCAL_SRC_FILES := \
	NativeExceptions.cpp \
	readTiffIncremental.cpp \
	NativeTiffBitmapFactory.cpp
LOCAL_LDLIBS := -ldl -llog -ljnigraphics
LOCAL_LDFLAGS +=-ljnigraphics
LOCAL_SHARED_LIBRARIES := tiff
include $(BUILD_SHARED_LIBRARY)

###############################################################
include $(CLEAR_VARS)
LOCAL_MODULE := tiffsaver
LOCAL_CFLAGS := -DANDROID_NDK
LOCAL_SRC_FILES := \
	NativeExceptions.cpp \
	NativeTiffSaver.cpp
LOCAL_LDLIBS := -ldl -llog -ljnigraphics
LOCAL_LDFLAGS +=-ljnigraphics
LOCAL_SHARED_LIBRARIES := tiff
include $(BUILD_SHARED_LIBRARY)