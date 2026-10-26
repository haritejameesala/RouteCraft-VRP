#include "RouteOptimizerJni.h"

#include "RouteOptimizer.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct NativeState {
    std::unordered_map<long long, std::shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer;

    NativeState() : optimizer(roadNodes) {}
};

void throwJava(JNIEnv* env, const char* className, const std::string& message) {
    if (env->ExceptionCheck()) {
        return;
    }
    jclass exceptionClass = env->FindClass(className);
    if (exceptionClass != nullptr) {
        env->ThrowNew(exceptionClass, message.c_str());
        env->DeleteLocalRef(exceptionClass);
    }
}

NativeState* getState(JNIEnv* env, jlong handle) {
    auto* state = reinterpret_cast<NativeState*>(static_cast<intptr_t>(handle));
    if (state == nullptr) {
        throwJava(env, "java/lang/IllegalStateException", "The native routing engine is closed.");
    }
    return state;
}

template <typename T>
bool hasLength(JNIEnv* env, jarray array, jsize expected, const char* name) {
    if (array == nullptr) {
        throwJava(env, "java/lang/IllegalArgumentException", std::string(name) + " must not be null.");
        return false;
    }
    if (env->GetArrayLength(array) != expected) {
        throwJava(env, "java/lang/IllegalArgumentException", std::string(name) + " has an invalid length.");
        return false;
    }
    return true;
}

std::vector<jint> readInts(JNIEnv* env, jintArray array) {
    jsize length = env->GetArrayLength(array);
    std::vector<jint> values(static_cast<size_t>(length));
    if (length > 0) {
        env->GetIntArrayRegion(array, 0, length, values.data());
    }
    return values;
}

std::vector<jlong> readLongs(JNIEnv* env, jlongArray array) {
    jsize length = env->GetArrayLength(array);
    std::vector<jlong> values(static_cast<size_t>(length));
    if (length > 0) {
        env->GetLongArrayRegion(array, 0, length, values.data());
    }
    return values;
}

std::vector<jdouble> readDoubles(JNIEnv* env, jdoubleArray array) {
    jsize length = env->GetArrayLength(array);
    std::vector<jdouble> values(static_cast<size_t>(length));
    if (length > 0) {
        env->GetDoubleArrayRegion(array, 0, length, values.data());
    }
    return values;
}

std::vector<jboolean> readBooleans(JNIEnv* env, jbooleanArray array) {
    jsize length = env->GetArrayLength(array);
    std::vector<jboolean> values(static_cast<size_t>(length));
    if (length > 0) {
        env->GetBooleanArrayRegion(array, 0, length, values.data());
    }
    return values;
}

std::string readString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        throwJava(env, "java/lang/IllegalArgumentException", "Depot names must not contain null entries.");
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

std::shared_ptr<Node> makeNode(int id, double x, double y, bool isDepot, int demand = 0, std::string name = {}) {
    return std::make_shared<Node>(id, std::move(name), x, y, demand, isDepot);
}

jdouble routeMetricValue(
    NativeState& state,
    const std::vector<jint>& ids,
    const std::vector<jboolean>& depotFlags,
    const std::vector<jdouble>& coordinates,
    jint metric) {
    if (ids.empty() || ids.size() != depotFlags.size() || coordinates.size() != ids.size() * 2
        || depotFlags[0] != JNI_TRUE || metric < 0 || metric > 3) {
        throw std::invalid_argument("Route metric input is invalid.");
    }

    auto depot = makeNode(ids[0], coordinates[0], coordinates[1], true);
    std::vector<std::shared_ptr<Node>> stops;
    stops.reserve(ids.size() - 1);
    for (size_t i = 1; i < ids.size(); ++i) {
        if (depotFlags[i] == JNI_TRUE) {
            throw std::invalid_argument("Only the first route node may be a depot.");
        }
        stops.push_back(makeNode(ids[i], coordinates[i * 2], coordinates[i * 2 + 1], false));
    }

    switch (metric) {
        case 0:
            return state.optimizer.routeKm(depot, stops);
        case 1:
            return state.optimizer.routeMinutes(depot, stops);
        case 2:
            return state.optimizer.routeCost(depot, stops);
        default:
            return state.optimizer.routeDistance(depot, stops);
    }
}

}  // namespace

JNIEXPORT jlong JNICALL Java_RouteOptimizerBridge_nativeCreate(JNIEnv* env, jclass) {
    try {
        auto state = std::make_unique<NativeState>();
        return static_cast<jlong>(reinterpret_cast<intptr_t>(state.release()));
    } catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Could not allocate the native routing engine.");
        return 0;
    }
}

JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeDestroy(JNIEnv*, jclass, jlong handle) {
    delete reinterpret_cast<NativeState*>(static_cast<intptr_t>(handle));
}

JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeSetWeights(
    JNIEnv* env, jclass, jlong handle, jdouble distance, jdouble time, jdouble cost) {
    NativeState* state = getState(env, handle);
    if (state != nullptr) {
        state->optimizer.setWeights(distance, time, cost);
    }
}

JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeSetRoadNetworkLoaded(
    JNIEnv* env, jclass, jlong handle, jboolean loaded) {
    NativeState* state = getState(env, handle);
    if (state != nullptr) {
        state->optimizer.setRoadNetworkLoaded(loaded == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL Java_RouteOptimizerBridge_nativeClearCache(JNIEnv* env, jclass, jlong handle) {
    NativeState* state = getState(env, handle);
    if (state != nullptr) {
        state->optimizer.clearCache();
    }
}

JNIEXPORT jlongArray JNICALL Java_RouteOptimizerBridge_nativeSetRoadGraph(
    JNIEnv* env,
    jclass,
    jlong handle,
    jlongArray nodeIds,
    jdoubleArray nodeValues,
    jintArray edgeFrom,
    jintArray edgeTo,
    jdoubleArray edgeValues) {
    NativeState* state = getState(env, handle);
    if (state == nullptr || nodeIds == nullptr) {
        return nullptr;
    }

    const jsize nodeCount = env->GetArrayLength(nodeIds);
    const jsize edgeCount = edgeFrom == nullptr ? -1 : env->GetArrayLength(edgeFrom);
    if (!hasLength<jdouble>(env, nodeValues, nodeCount * 4, "nodeValues")
        || !hasLength<jint>(env, edgeFrom, edgeCount, "edgeFrom")
        || !hasLength<jint>(env, edgeTo, edgeCount, "edgeTo")
        || !hasLength<jdouble>(env, edgeValues, edgeCount * 3, "edgeValues")) {
        return nullptr;
    }

    std::vector<jlong> ids = readLongs(env, nodeIds);
    std::vector<jdouble> values = readDoubles(env, nodeValues);
    std::vector<jint> from = readInts(env, edgeFrom);
    std::vector<jint> to = readInts(env, edgeTo);
    std::vector<jdouble> edgeMetrics = readDoubles(env, edgeValues);
    if (env->ExceptionCheck()) {
        return nullptr;
    }

    std::vector<std::shared_ptr<RoadNode>> nodes;
    nodes.reserve(ids.size());
    std::unordered_map<long long, std::shared_ptr<RoadNode>> nextRoadNodes;
    for (size_t i = 0; i < ids.size(); ++i) {
        auto node = std::make_shared<RoadNode>(ids[i], values[i * 4], values[i * 4 + 1]);
        node->x = values[i * 4 + 2];
        node->y = values[i * 4 + 3];
        if (!nextRoadNodes.emplace(node->id, node).second) {
            throwJava(env, "java/lang/IllegalArgumentException", "Road node identifiers must be unique.");
            return nullptr;
        }
        nodes.push_back(std::move(node));
    }
    for (size_t i = 0; i < from.size(); ++i) {
        if (from[i] < 0 || to[i] < 0 || static_cast<size_t>(from[i]) >= nodes.size()
            || static_cast<size_t>(to[i]) >= nodes.size()) {
            throwJava(env, "java/lang/IllegalArgumentException", "Road edge references an unknown node.");
            return nullptr;
        }
        nodes[from[i]]->edges.emplace_back(
            nodes[to[i]], edgeMetrics[i * 3], edgeMetrics[i * 3 + 1], edgeMetrics[i * 3 + 2]);
    }

    std::set<std::shared_ptr<RoadNode>> retained(nodes.begin(), nodes.end());
    RouteOptimizer::retainLargestStronglyConnectedComponent(retained);
    state->roadNodes.clear();
    for (const auto& node : retained) {
        state->roadNodes.emplace(node->id, node);
    }
    state->optimizer.clearCache();

    std::vector<jlong> retainedIds;
    retainedIds.reserve(retained.size());
    for (const auto& node : retained) {
        retainedIds.push_back(node->id);
    }
    jlongArray result = env->NewLongArray(static_cast<jsize>(retainedIds.size()));
    if (result != nullptr && !retainedIds.empty()) {
        env->SetLongArrayRegion(result, 0, static_cast<jsize>(retainedIds.size()), retainedIds.data());
    }
    return result;
}

JNIEXPORT jlong JNICALL Java_RouteOptimizerBridge_nativeNearestRoadNode(
    JNIEnv* env, jclass, jlong handle, jdouble x, jdouble y) {
    NativeState* state = getState(env, handle);
    if (state == nullptr) {
        return 0;
    }
    std::shared_ptr<RoadNode> node = state->optimizer.nearestRoadNode(x, y);
    return node == nullptr ? 0 : static_cast<jlong>(node->id);
}

JNIEXPORT jobjectArray JNICALL Java_RouteOptimizerBridge_nativeOptimize(
    JNIEnv* env,
    jclass,
    jlong handle,
    jintArray customerIds,
    jintArray customerDemands,
    jdoubleArray customerCoordinates,
    jintArray depotIds,
    jdoubleArray depotCoordinates,
    jobjectArray depotNames,
    jint vehicleCount,
    jint capacity,
    jboolean useSavings) {
    NativeState* state = getState(env, handle);
    if (state == nullptr || customerIds == nullptr || depotIds == nullptr) {
        return nullptr;
    }
    const jsize customerCount = env->GetArrayLength(customerIds);
    const jsize depotCount = env->GetArrayLength(depotIds);
    if (!hasLength<jint>(env, customerDemands, customerCount, "customerDemands")
        || !hasLength<jdouble>(env, customerCoordinates, customerCount * 2, "customerCoordinates")
        || !hasLength<jdouble>(env, depotCoordinates, depotCount * 2, "depotCoordinates")
        || !hasLength<jobject>(env, depotNames, depotCount, "depotNames")) {
        return nullptr;
    }

    std::vector<jint> customers = readInts(env, customerIds);
    std::vector<jint> demands = readInts(env, customerDemands);
    std::vector<jdouble> customerPoints = readDoubles(env, customerCoordinates);
    std::vector<jint> depots = readInts(env, depotIds);
    std::vector<jdouble> depotPoints = readDoubles(env, depotCoordinates);
    if (env->ExceptionCheck()) {
        return nullptr;
    }

    std::vector<std::shared_ptr<Node>> customerNodes;
    customerNodes.reserve(customers.size());
    std::unordered_map<int, int> customerIndexes;
    for (size_t i = 0; i < customers.size(); ++i) {
        if (!customerIndexes.emplace(customers[i], static_cast<int>(i)).second) {
            throwJava(env, "java/lang/IllegalArgumentException", "Customer identifiers must be unique.");
            return nullptr;
        }
        customerNodes.push_back(makeNode(
            customers[i], customerPoints[i * 2], customerPoints[i * 2 + 1], false, demands[i]));
    }

    std::vector<std::shared_ptr<Node>> depotNodes;
    depotNodes.reserve(depots.size());
    std::unordered_map<int, int> depotIndexes;
    for (size_t i = 0; i < depots.size(); ++i) {
        if (!depotIndexes.emplace(depots[i], static_cast<int>(i)).second) {
            throwJava(env, "java/lang/IllegalArgumentException", "Depot identifiers must be unique.");
            return nullptr;
        }
        auto name = static_cast<jstring>(env->GetObjectArrayElement(depotNames, static_cast<jsize>(i)));
        if (env->ExceptionCheck()) {
            return nullptr;
        }
        std::string depotName = readString(env, name);
        env->DeleteLocalRef(name);
        if (env->ExceptionCheck()) {
            return nullptr;
        }
        depotNodes.push_back(makeNode(
            depots[i], depotPoints[i * 2], depotPoints[i * 2 + 1], true, 0, std::move(depotName)));
    }

    try {
        std::vector<Route> routes = state->optimizer.optimize(
            customerNodes, depotNodes, vehicleCount, capacity, useSavings == JNI_TRUE);
        jclass intArrayClass = env->FindClass("[I");
        if (intArrayClass == nullptr) {
            return nullptr;
        }
        jobjectArray result = env->NewObjectArray(static_cast<jsize>(routes.size()), intArrayClass, nullptr);
        env->DeleteLocalRef(intArrayClass);
        if (result == nullptr) {
            return nullptr;
        }
        for (size_t i = 0; i < routes.size(); ++i) {
            auto depotIndex = depotIndexes.find(routes[i].depot->id);
            if (depotIndex == depotIndexes.end()) {
                throw std::logic_error("The optimizer returned a route for an unknown depot.");
            }
            std::vector<jint> encoded;
            encoded.reserve(routes[i].stops.size() + 1);
            encoded.push_back(depotIndex->second);
            for (const auto& stop : routes[i].stops) {
                auto customerIndex = customerIndexes.find(stop->id);
                if (customerIndex == customerIndexes.end()) {
                    throw std::logic_error("The optimizer returned an unknown customer.");
                }
                encoded.push_back(customerIndex->second);
            }
            jintArray route = env->NewIntArray(static_cast<jsize>(encoded.size()));
            if (route == nullptr) {
                return nullptr;
            }
            env->SetIntArrayRegion(route, 0, static_cast<jsize>(encoded.size()), encoded.data());
            env->SetObjectArrayElement(result, static_cast<jsize>(i), route);
            env->DeleteLocalRef(route);
            if (env->ExceptionCheck()) {
                return nullptr;
            }
        }
        return result;
    } catch (const std::exception& error) {
        throwJava(env, "java/lang/IllegalArgumentException", error.what());
        return nullptr;
    }
}

JNIEXPORT jdouble JNICALL Java_RouteOptimizerBridge_nativeRouteMetric(
    JNIEnv* env,
    jclass,
    jlong handle,
    jintArray nodeIds,
    jbooleanArray depotFlags,
    jdoubleArray coordinates,
    jint metric) {
    NativeState* state = getState(env, handle);
    if (state == nullptr || nodeIds == nullptr) {
        return 0.0;
    }
    const jsize count = env->GetArrayLength(nodeIds);
    if (!hasLength<jboolean>(env, depotFlags, count, "depotFlags")
        || !hasLength<jdouble>(env, coordinates, count * 2, "coordinates")) {
        return 0.0;
    }
    std::vector<jint> ids = readInts(env, nodeIds);
    std::vector<jboolean> flags = readBooleans(env, depotFlags);
    std::vector<jdouble> points = readDoubles(env, coordinates);
    if (env->ExceptionCheck()) {
        return 0.0;
    }
    try {
        return routeMetricValue(*state, ids, flags, points, metric);
    } catch (const std::exception& error) {
        throwJava(env, "java/lang/IllegalArgumentException", error.what());
        return 0.0;
    }
}

JNIEXPORT jobjectArray JNICALL Java_RouteOptimizerBridge_nativeLeg(
    JNIEnv* env,
    jclass,
    jlong handle,
    jint fromId,
    jboolean fromDepot,
    jdouble fromX,
    jdouble fromY,
    jint toId,
    jboolean toDepot,
    jdouble toX,
    jdouble toY) {
    NativeState* state = getState(env, handle);
    if (state == nullptr) {
        return nullptr;
    }
    Leg result = state->optimizer.leg(
        makeNode(fromId, fromX, fromY, fromDepot == JNI_TRUE),
        makeNode(toId, toX, toY, toDepot == JNI_TRUE));

    jclass objectClass = env->FindClass("java/lang/Object");
    if (objectClass == nullptr) {
        return nullptr;
    }
    jobjectArray response = env->NewObjectArray(2, objectClass, nullptr);
    env->DeleteLocalRef(objectClass);
    if (response == nullptr) {
        return nullptr;
    }

    jdouble metrics[4] = {result.km, result.minutes, result.cost, result.score};
    jdoubleArray metricArray = env->NewDoubleArray(4);
    std::vector<jlong> pathIds;
    pathIds.reserve(result.path.size());
    for (const auto& node : result.path) {
        pathIds.push_back(node->id);
    }
    jlongArray pathArray = env->NewLongArray(static_cast<jsize>(pathIds.size()));
    if (metricArray == nullptr || pathArray == nullptr) {
        return nullptr;
    }
    env->SetDoubleArrayRegion(metricArray, 0, 4, metrics);
    if (!pathIds.empty()) {
        env->SetLongArrayRegion(pathArray, 0, static_cast<jsize>(pathIds.size()), pathIds.data());
    }
    env->SetObjectArrayElement(response, 0, metricArray);
    env->SetObjectArrayElement(response, 1, pathArray);
    env->DeleteLocalRef(metricArray);
    env->DeleteLocalRef(pathArray);
    return response;
}

JNIEXPORT jdouble JNICALL Java_RouteOptimizerBridge_nativeGeoKm(
    JNIEnv*, jclass, jdouble lat1, jdouble lon1, jdouble lat2, jdouble lon2) {
    return RouteOptimizer::geoKm(lat1, lon1, lat2, lon2);
}
