//
// Created by maks on 20.09.2025.
//

#include <jni.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "load_stages.h"

#define TAG __FILE_NAME__
#include "log.h"


typedef jint (*JNI_CreateJavaVM_t)(JavaVM** p_vm, JNIEnv** p_env, void* vm_args);

typedef struct {
    void* handle;
    JNI_CreateJavaVM_t JNI_CreateJavaVM;
    JNIEnv *vm_env;
    JavaVM *vm;
} java_vm_t;

struct {
    JavaVM *host_vm;
    jmethodID host_exit_method;
    jclass host_exit_class;
    jobject vm_exit_context;
} vm_exit_data;

void throwException(JNIEnv *env, jint loadStage, jint errorCode, const char* errorInfo) {
    jclass loadException = (*env)->FindClass(env, "net/kdt/pojavlaunch/utils/jre/VMLoadException");
    if(loadException == NULL) {
        jthrowable exception = (*env)->ExceptionOccurred(env);
        (*env)->Throw(env, exception);
        return;
    }
    jmethodID loadException_constructor = (*env)->GetMethodID(env, loadException, "<init>", "(Ljava/lang/String;II)V");
    if(errorInfo == NULL) errorInfo = "unspecified";
    jstring infoString = (*env)->NewStringUTF(env, errorInfo);
    jthrowable throwable = (*env)->NewObject(env,loadException, loadException_constructor, infoString, loadStage, errorCode);
    (*env)->DeleteLocalRef(env, infoString);
    (*env)->DeleteLocalRef(env, loadException);
    (*env)->Throw(env, throwable);
}

_Noreturn static void callExit(int code, bool isAbort) {
    JavaVM *vm = vm_exit_data.host_vm;
    JNIEnv *env;
    jint result = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if(result == JNI_EDETACHED) {
        result = (*vm)->AttachCurrentThread(vm, &env, NULL);
    }
    if(result != JNI_OK) {
        abort();
    }
    // This will call System.exit()
    (*env)->CallStaticVoidMethod(env, vm_exit_data.host_exit_class, vm_exit_data.host_exit_method, vm_exit_data.vm_exit_context, code, isAbort);
    while (true) {}
}

_Noreturn static void vm_exit(int code) {
    callExit(code, false);
}

_Noreturn static void vm_abort() {
    callExit(0, true);
}

static bool loadJavaVM(java_vm_t* java_vm, const char* jvm_path) {
    java_vm->handle = dlopen(jvm_path, RTLD_NOW | RTLD_GLOBAL);
    if(java_vm->handle == NULL) return false;
#define JVM_LOAD(name) java_vm->name = dlsym(java_vm->handle, #name)
    JVM_LOAD(JNI_CreateJavaVM);
#undef JVM_LOAD
    return java_vm->JNI_CreateJavaVM != NULL;
}


static bool initializeJavaVM(java_vm_t* java_vm, JNIEnv *env, jstring* vmpath, jobjectArray java_args) {
    char* fail_msg;
#define FAIL(msg) {fail_msg = msg; goto fail;}

    const char* jvm_path = (*env)->GetStringUTFChars(env, vmpath, NULL);
    if(!loadJavaVM(java_vm, jvm_path)) {
        throwException(env, STAGE_LOAD_RUNTIME, JNI_ERR, dlerror());
        return false;
    }

    jint userArgsCount = (*env)->GetArrayLength(env, java_args);
    jint javaVmArgsCount = userArgsCount + 2; // for exit and abort hooks
    JavaVMOption javaVmOptions[javaVmArgsCount];

    const char** user_args = convert_to_char_array(env, java_args);
    if(user_args == NULL) FAIL("Failed to read user arguments")
    for(jint i = 0; i < userArgsCount; i++) {
        const char* arg = user_args[i];
        LOGI("VM arg: %s",arg);
        if(arg == NULL) FAIL("Unexpected NULL argument")
        javaVmOptions[i].optionString = arg;
    }

    javaVmOptions[userArgsCount].optionString = "exit";
    javaVmOptions[userArgsCount].extraInfo = vm_exit;
    javaVmOptions[userArgsCount + 1].optionString = "abort";
    javaVmOptions[userArgsCount + 1].extraInfo = vm_abort;

    JavaVMInitArgs initArgs;
    initArgs.nOptions = javaVmArgsCount;
    initArgs.options = javaVmOptions;
    initArgs.ignoreUnrecognized = JNI_TRUE;
    initArgs.version = JNI_VERSION_1_6;

    jint result = java_vm->JNI_CreateJavaVM(&java_vm->vm, &java_vm->vm_env, &initArgs);

    free_char_array(env, java_args, user_args);

    if(result < 0) {
        dlclose(java_vm->handle);
        throwException(env, STAGE_CREATE_RUNTIME, result, NULL);
        return false;
    }
    return true;

    fail:
    if(user_args != NULL) free_char_array(env, java_args, user_args);
    dlclose(java_vm->handle);
    throwException(env, STAGE_CREATE_RUNTIME, JNI_ERR, fail_msg);
    return false;
#undef FAIL
}

static jobject initalizeClassLoader(JNIEnv *env, JNIEnv* vm_env, jobjectArray classpath) {
#define EXCEPTION_CHECK if((*vm_env)->ExceptionCheck(vm_env)) {(*vm_env)->ExceptionDescribe(vm_env); return NULL;}
    jclass vmc_url = (*vm_env)->FindClass(vm_env, "java/net/URL"); EXCEPTION_CHECK
    jclass vmc_classloader = (*vm_env)->FindClass(vm_env, "java/net/URLClassLoader"); EXCEPTION_CHECK

    jmethodID vm_url_constructor = (*vm_env)->GetMethodID(vm_env, vmc_url, "<init>", "(Ljava/lang/String;)V"); EXCEPTION_CHECK
    jmethodID vm_url_constructor_context = (*vm_env)->GetMethodID(vm_env, vmc_url, "<init>", "(Ljava/net/URL;Ljava/lang/String;)V"); EXCEPTION_CHECK
    jmethodID vm_urlclassloader_constructor = (*vm_env)->GetMethodID(vm_env, vmc_classloader, "<init>", "([Ljava/net/URL;)V"); EXCEPTION_CHECK

    jstring baseContextString = (*vm_env)->NewStringUTF(vm_env, "file://"); EXCEPTION_CHECK
    jobject contextUrl = (*vm_env)->NewObject(vm_env, vmc_url, vm_url_constructor, baseContextString); EXCEPTION_CHECK
    (*vm_env)->DeleteLocalRef(vm_env, baseContextString);

    jint classpathEntries = (*env)->GetArrayLength(env, classpath);
    jobjectArray classpathUrls = (*vm_env)->NewObjectArray(vm_env, classpathEntries, vmc_url, NULL); EXCEPTION_CHECK

    for(jint i = 0; i < classpathEntries; i++) {
        jstring classpathEntryObj = (*env)->GetObjectArrayElement(env, classpath, i);
        if(classpathEntryObj == NULL) return NULL;
        const char* classpathEntry = (*env)->GetStringUTFChars(env, classpathEntryObj, NULL);
        if(classpathEntry == NULL) return NULL;
        jstring classpathEntryVm = (*vm_env)->NewStringUTF(vm_env, classpathEntry); EXCEPTION_CHECK
        jobject classpathEntryUrl = (*vm_env)->NewObject(vm_env, vmc_url, vm_url_constructor_context, contextUrl, classpathEntryVm); EXCEPTION_CHECK
        (*env)->ReleaseStringUTFChars(env, classpathEntryObj, classpathEntry);
        (*env)->DeleteLocalRef(env, classpathEntryObj);
        (*vm_env)->SetObjectArrayElement(vm_env, classpathUrls, i, classpathEntryUrl); EXCEPTION_CHECK
        (*vm_env)->DeleteLocalRef(vm_env, classpathEntryUrl);
    }

    jobject urlClassLoader = (*vm_env)->NewObject(vm_env, vmc_classloader, vm_urlclassloader_constructor, classpathUrls); EXCEPTION_CHECK
    (*vm_env)->DeleteLocalRef(vm_env, classpathUrls);
    return urlClassLoader;
#undef EXCEPTION_CHECK
}

static bool executeMain(JNIEnv* vm_env, jobject urlClassLoader, const char* mainClass, jobjectArray vm_appArgs) {
#define EXCEPTION_CHECK if((*vm_env)->ExceptionCheck(vm_env)) {(*vm_env)->ExceptionDescribe(vm_env); return false;}
    jclass classLoaderClass = (*vm_env)->GetObjectClass(vm_env, urlClassLoader); EXCEPTION_CHECK
    jmethodID classloader_loadClass = (*vm_env)->GetMethodID(vm_env, classLoaderClass, "loadClass", "(Ljava/lang/String;Z)Ljava/lang/Class;"); EXCEPTION_CHECK
    (*vm_env)->DeleteLocalRef(vm_env, classLoaderClass);
    jstring className = (*vm_env)->NewStringUTF(vm_env, mainClass); EXCEPTION_CHECK
    jclass mainClassObj = (jclass) (*vm_env)->CallObjectMethod(vm_env, urlClassLoader, classloader_loadClass, className, JNI_TRUE); EXCEPTION_CHECK
    (*vm_env)->DeleteLocalRef(vm_env, className);
    jmethodID mainMethod = (*vm_env)->GetStaticMethodID(vm_env, mainClassObj, "main", "([Ljava/lang/String;)V"); EXCEPTION_CHECK
    (*vm_env)->CallStaticVoidMethod(vm_env, mainClassObj, mainMethod, vm_appArgs); EXCEPTION_CHECK
    return true;
#undef EXCEPTION_CHECK
}

static void unloadJavaVM(java_vm_t* java_vm) {
    JavaVM *vm = java_vm->vm;
    (*vm)->DestroyJavaVM(vm);
    dlclose(java_vm->handle);
}



extern bool installClassLoaderHooks(JNIEnv *env, JNIEnv* vm_env);

JNIEXPORT jboolean JNICALL
Java_net_kdt_pojavlaunch_utils_jre_JavaRunner_nativeLoadJVM(JNIEnv *env, jclass clazz, jstring vmpath, jobjectArray java_args, jobjectArray classpath, jstring mainClass, jobjectArray appArgs) {
    java_vm_t java_vm;
    if(!initializeJavaVM(&java_vm, env, vmpath, java_args)) return JNI_FALSE;
    JNIEnv *vm_env = java_vm.vm_env;

    if(!installClassLoaderHooks(env, vm_env)) return JNI_FALSE;

    jobject urlClassLoader = initalizeClassLoader(env, vm_env, classpath);
    if(urlClassLoader == NULL) {
        unloadJavaVM(&java_vm);
        throwException(env, STAGE_LOAD_CLASSPATH, JNI_ERR, "Failed to create the class loader. Check latestlog.txt");
        return JNI_FALSE;
    }

    jint numAppArgs = (*env)->GetArrayLength(env, appArgs);
    const char** appArgsChar = convert_to_char_array(env, appArgs);
    jobjectArray vm_appArgs = convert_from_char_array(vm_env, appArgsChar, numAppArgs);
    free_char_array(env, appArgs, appArgsChar);

    const char* mainClassNameBuf = (*env)->GetStringUTFChars(env, mainClass, NULL);
    size_t mainClassLen = strlen(mainClassNameBuf) + 1;
    char mainClassName[mainClassLen + 1];
    strncpy(mainClassName, mainClassNameBuf, mainClassLen + 1);
    (*env)->ReleaseStringUTFChars(env, mainClass, mainClassNameBuf);

    if(!executeMain(vm_env, urlClassLoader, mainClassName, vm_appArgs)) {
        unloadJavaVM(&java_vm);
        throwException(env, STAGE_RUN_MAIN, JNI_ERR, "Failed to start the main class. Check latestlog.txt");
        return JNI_FALSE;
    }
    return JNI_TRUE;
}


JNIEXPORT void JNICALL
Java_net_kdt_pojavlaunch_utils_jre_JavaRunner_nativeSetupExit(JNIEnv *env, jclass clazz,
                                                              jobject context) {
    (*env)->GetJavaVM(env, &vm_exit_data.host_vm);
    jclass class = (*env)->FindClass(env, "net/kdt/pojavlaunch/ExitActivity");
    vm_exit_data.host_exit_class  = (*env)->NewGlobalRef(env, class);
    vm_exit_data.host_exit_method = (*env)->GetStaticMethodID(env, class, "showExitMessage", "(Landroid/content/Context;IZ)V");
    vm_exit_data.vm_exit_context = (*env)->NewGlobalRef(env, context);
}