//
// Created by maks on 20.09.2025.
//

#include <jni.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <stdlib.h>
#include "elf_hinter.h"
#include "load_stages.h"

#define TAG __FILE_NAME__
#include <log.h>



// Java 17+ style hook
typedef jboolean (*NativeLibraries_load)(JNIEnv *env, jclass cls, jobject lib, jstring name, jboolean isBuiltin, jboolean throwExceptionIfFail);

// Java 8 style hook
typedef void(*ClassLoader_00024NativeLibrary_load)(JNIEnv *env, jobject this, jstring name, jboolean isBuiltin);

union {
    NativeLibraries_load j17p;
    ClassLoader_00024NativeLibrary_load j8;
} original_func;



static jboolean hook_NativeLibraries_load(JNIEnv *env, jclass cls, jobject lib, jstring name, jboolean isBuiltin, jboolean throwExceptionIfFail) {
    const char* name_n = (*env)->GetStringUTFChars(env, name, NULL);
    hinter_t hinter;
    hinter_process(&hinter, name_n);
    (*env)->ReleaseStringUTFChars(env, name, name_n);
    jboolean result = original_func.j17p(env, cls, lib, name, isBuiltin, throwExceptionIfFail);
    hinter_free(&hinter);
    return result;
}

static void hook_ClassLoader_00024NativeLibrary_load(JNIEnv *env, jobject this, jstring name, jboolean isBuiltin) {
    const char* name_n = (*env)->GetStringUTFChars(env, name, NULL);
    hinter_t hinter;
    hinter_process(&hinter, name_n);
    (*env)->ReleaseStringUTFChars(env, name, name_n);
    original_func.j8(env, this, name, isBuiltin);
    hinter_free(&hinter);
}

bool installClassLoaderHooks(JNIEnv *env, JNIEnv* vm_env) {
    void* libjava = dlopen("libjava.so", RTLD_NOLOAD);
    if(libjava == NULL) {
        throwException(env, STAGE_FIND_HOOKS_NATIVE, JNI_ERR, "Failed to find libjava.so after VM startup");
        return false;
    }
    const char* hookClass;
    JNINativeMethod hookMethod;
    void* hookFunc_j17 = dlsym(libjava, "Java_jdk_internal_loader_NativeLibraries_load");
    void* hookFunc_j8 = dlsym(libjava, "Java_java_lang_ClassLoader_00024NativeLibrary_load");
    if(hookFunc_j17 != NULL) {
        hookClass = "jdk/internal/loader/NativeLibraries";
        hookMethod.name = "load";
        hookMethod.signature = "(Ljdk/internal/loader/NativeLibraries$NativeLibraryImpl;Ljava/lang/String;ZZ)Z";
        hookMethod.fnPtr = hook_NativeLibraries_load;
        original_func.j17p = hookFunc_j17;
    }else if(hookFunc_j8 != NULL) {
        hookClass = "java/lang/ClassLoader$NativeLibrary";
        hookMethod.name = "load";
        hookMethod.signature = "(Ljava/lang/String;Z)V";
        hookMethod.fnPtr = hook_ClassLoader_00024NativeLibrary_load;
        original_func.j8 = hookFunc_j8;
    }
    jclass hookedClass = (*vm_env)->FindClass(vm_env, hookClass);
    if(hookedClass == NULL) {
        if((*vm_env)->ExceptionCheck(vm_env)) (*vm_env)->ExceptionDescribe(vm_env);
        throwException(env, STAGE_FIND_HOOKS, JNI_ERR, "Cannot find hook target. Check latestlog.txt");
        return false;
    }
    jint result = (*vm_env)->RegisterNatives(vm_env, hookedClass, &hookMethod, 1);
    if(result != JNI_OK) {
        if((*vm_env)->ExceptionCheck(vm_env)) (*vm_env)->ExceptionDescribe(vm_env);
        throwException(env, STAGE_INSERT_HOOKS, result, "Cannot register hooks");
        return false;
    }
    return true;
}