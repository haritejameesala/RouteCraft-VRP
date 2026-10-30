#ifndef ROUTE_OPTIMIZER_JNI_H
#define ROUTE_OPTIMIZER_JNI_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// Creates the native optimizer state and returns its opaque handle.
JNIEXPORT jlong JNICALL Java_RouteOptimizerBridge_nativeCreate(
    JNIEnv*, jclass);
// Releases the native state associated with a handle.
JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeDestroy(
    JNIEnv*, jclass, jlong);
// Updates the optimizer's distance, time, and cost score weights.
JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeSetWeights(
    JNIEnv*, jclass, jlong, jdouble, jdouble, jdouble);
// Enables or disables road-graph-based leg calculations.
JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeSetRoadNetworkLoaded(
    JNIEnv*, jclass, jlong, jboolean);
// Clears cached leg calculations for the selected optimizer.
JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeClearCache(
    JNIEnv*, jclass, jlong);
// Loads a road graph and returns the identifiers in its largest connected component.
JNIEXPORT jlongArray JNICALL Java_RouteOptimizerBridge_nativeSetRoadGraph(
    JNIEnv*, jclass, jlong, jlongArray, jdoubleArray, jintArray, jintArray,
    jdoubleArray);
// Returns the road-graph node nearest to projected coordinates.
JNIEXPORT jlong JNICALL Java_RouteOptimizerBridge_nativeNearestRoadNode(
    JNIEnv*, jclass, jlong, jdouble, jdouble);
// Optimizes customer routes and returns each route as depot/customer indexes.
JNIEXPORT jobjectArray JNICALL Java_RouteOptimizerBridge_nativeOptimize(
    JNIEnv*, jclass, jlong, jintArray, jintArray, jdoubleArray, jintArray,
    jdoubleArray, jobjectArray, jint, jint, jboolean);
// Measures a supplied route using the requested distance, time, cost, or score.
JNIEXPORT jdouble JNICALL Java_RouteOptimizerBridge_nativeRouteMetric(
    JNIEnv*, jclass, jlong, jintArray, jbooleanArray, jdoubleArray, jint);
// Returns leg metrics and the road-node path between two locations.
JNIEXPORT jobjectArray JNICALL Java_RouteOptimizerBridge_nativeLeg(
    JNIEnv*, jclass, jlong, jint, jboolean, jdouble, jdouble, jint, jboolean,
    jdouble, jdouble);
// Computes great-circle distance for the Java bridge.
JNIEXPORT jdouble JNICALL Java_RouteOptimizerBridge_nativeGeoKm(
    JNIEnv*, jclass, jdouble, jdouble, jdouble, jdouble);

#ifdef __cplusplus
}
#endif

#endif
