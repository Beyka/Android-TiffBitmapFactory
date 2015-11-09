//
// Created by alexeyba on 09.11.15.
//

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TIFFEXAMPLE_NATIVEEXCEPTIONS_H
#define TIFFEXAMPLE_NATIVEEXCEPTIONS_H

#endif //TIFFEXAMPLE_NATIVEEXCEPTIONS_H

#include <jni.h>



void throw_not_enought_memory_exception(JNIEnv *, int);
void throw_read_file_exception(JNIEnv *, jstring);
void throw_no_such_file_exception(JNIEnv *, jstring);

#ifdef __cplusplus
}
#endif

