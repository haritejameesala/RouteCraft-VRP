#ifndef ROUTE_OPTIMIZER_JNI_H
#define ROUTE_OPTIMIZER_JNI_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL Java_RouteOptimizerBridge_nativeCreate(JNIEnv*, jclass);
JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeDestroy(JNIEnv*, jclass, jlong);
JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeSetWeights(JNIEnv*, jclass, jlong, jdouble, jdouble, jdouble);
JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeSetRoadNetworkLoaded(JNIEnv*, jclass, jlong, jboolean);
JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeClearCache(JNIEnv*, jclass, jlong);
JNIEXPORT jlongArray JNICALL Java_RouteOptimizerBridge_nativeSetRoadGraph(
    JNIEnv*, jclass, jlong, jlongArray, jdoubleArray, jintArray, jintArray, jdoubleArray);
JNIEXPORT jlong JNICALL Java_RouteOptimizerBridge_nativeNearestRoadNode(JNIEnv*, jclass, jlong, jdouble, jdouble);
JNIEXPORT jobjectArray JNICALL Java_RouteOptimizerBridge_nativeOptimize(
    JNIEnv*, jclass, jlong, jintArray, jintArray, jdoubleArray,
    jintArray, jdoubleArray, jobjectArray, jint, jint, jboolean);
JNIEXPORT jdouble JNICALL Java_RouteOptimizerBridge_nativeRouteMetric(
    JNIEnv*, jclass, jlong, jintArray, jbooleanArray, jdoubleArray, jint);
JNIEXPORT jobjectArray JNICALL Java_RouteOptimizerBridge_nativeLeg(
    JNIEnv*, jclass, jlong, jint, jboolean, jdouble, jdouble, jint, jboolean, jdouble, jdouble);
JNIEXPORT jdouble JNICALL Java_RouteOptimizerBridge_nativeGeoKm(
    JNIEnv*, jclass, jdouble, jdouble, jdouble, jdouble);

#ifdef __cplusplus
}
#endif

#endif
