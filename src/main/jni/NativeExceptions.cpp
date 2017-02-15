//
// Created by alexeyba on 09.11.15.
//
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#include "NativeExceptions.h"

void throw_not_enought_memory_exception(JNIEnv *env, int available, int need)
{
    jclass exClass;
    jmethodID exConstructorID;
    jobject exObj;
    char *className = "org/beyka/tiffbitmapfactory/exceptions/NotEnoughtMemoryException" ;

    exClass = env->FindClass(className);

    exConstructorID = env->GetMethodID(exClass, "<init>", "(II)V");

    exObj = env->NewObject(exClass, exConstructorID, available, need);

    env->Throw((jthrowable)exObj);
}

void throw_decode_file_exception(JNIEnv *env, jstring str)
{
    jclass exClass;
    jmethodID exConstructorID;
    jobject exObj;
    char *className = "org/beyka/tiffbitmapfactory/exceptions/DecodeTiffException" ;

    exClass = env->FindClass(className);

    exConstructorID = env->GetMethodID(exClass, "<init>", "(Ljava/lang/String;)V");

    //Create java string
//    jstring exString = env->NewStringUTF(str);

    exObj = env->NewObject(exClass, exConstructorID, str);

    env->Throw((jthrowable)exObj);
}

void throw_no_such_file_exception(JNIEnv *env, jstring str)
{
    jclass exClass;
    jmethodID exConstructorID;
    jobject exObj;
    char *className = "org/beyka/tiffbitmapfactory/exceptions/NoSuchFileException" ;

    exClass = env->FindClass(className);

    exConstructorID = env->GetMethodID(exClass, "<init>", "(Ljava/lang/String;)V");

    exObj = env->NewObject(exClass, exConstructorID, str);

    env->Throw((jthrowable)exObj);
}

#ifdef __cplusplus
}
#endif

