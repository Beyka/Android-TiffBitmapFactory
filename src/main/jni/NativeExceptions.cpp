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
    const char *className = "org/beyka/tiffbitmapfactory/exceptions/NotEnoughtMemoryException" ;

    exClass = env->FindClass(className);

    exConstructorID = env->GetMethodID(exClass, "<init>", "(II)V");

    exObj = env->NewObject(exClass, exConstructorID, available, need);

    env->Throw((jthrowable)exObj);
}

void throw_decode_file_exception(JNIEnv *env, jstring str, jstring additionalInfo)
{
    jclass exClass;
    jmethodID exConstructorID;
    jobject exObj;
    const char *className = "org/beyka/tiffbitmapfactory/exceptions/DecodeTiffException" ;

    /*LOGIS("created", aditionalInfo);
    LOGI("creating jstring");
    jstring info = env->NewStringUTF(aditionalInfo);
    LOGIS("created", info);
*/
    exClass = env->FindClass(className);

    exConstructorID = env->GetMethodID(exClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");

    //Create java string
//    jstring exString = env->NewStringUTF(str);

    exObj = env->NewObject(exClass, exConstructorID, str, additionalInfo);

    env->Throw((jthrowable)exObj);
}

void throw_cant_open_file_exception(JNIEnv *env, jstring str)
{
    jclass exClass;
    jmethodID exConstructorID;
    jobject exObj;
    const char *className = "org/beyka/tiffbitmapfactory/exceptions/CantOpenFileException" ;

    exClass = env->FindClass(className);

    exConstructorID = env->GetMethodID(exClass, "<init>", "(Ljava/lang/String;)V");

    exObj = env->NewObject(exClass, exConstructorID, str);

    env->Throw((jthrowable)exObj);
}

#ifdef __cplusplus
}
#endif

