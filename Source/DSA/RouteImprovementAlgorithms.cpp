// File purpose: Validates, scores, measures, and improves the constructed routes.
// DSA part: Part 2 - Route improvement and evaluation.
#include "RouteOptimizer.h"
#include "RoadNetworkAlgorithms.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

using namespace std;

namespace {

constexpr double TWO_OPT_EPSILON = 1e-9;

}  // namespace

// Creates an optimizer bound to the road graph owned by the caller.
RouteOptimizer::RouteOptimizer(
    unordered_map<long long, shared_ptr<RoadNode>>& roadNodes)
    : roadNodes(roadNodes) {}

// Updates the metric weights and clears scores calculated with the old weights.
void RouteOptimizer::setWeights(double d, double t, double c) {
    weightDistance = d;
    weightTime = t;
    weightCost = c;
    clearCache();
}

// Removes all memoized legs so future requests use current routing data.
void RouteOptimizer::clearCache() {
    legCache.clear();
}

// Selects graph routing or straight-line fallback for future legs.
void RouteOptimizer::setRoadNetworkLoaded(bool loaded) {
    roadNetworkLoaded = loaded;
    clearCache();
}

// Converts the selected leg into the score used when comparing route choices.
double RouteOptimizer::distance(
    const shared_ptr<Node>& first, const shared_ptr<Node>& second) {
    return leg(first, second).score;
}

// Normalizes and combines distance, time, and cost into one objective value.
double RouteOptimizer::objectiveScore(
    double km, double minutes, double cost) const {
    return weightDistance * km + weightTime * (minutes / 60.0 * 30.0) +
           weightCost * (cost / 12.0);
}

// Validates inputs, builds routes with the selected heuristic, and improves them.
vector<Route> RouteOptimizer::optimize(
    const vector<shared_ptr<Node>>& customers,
    const vector<shared_ptr<Node>>& depots, int vehicles, int vehicleCapacity,
    bool useSavings) {
    return optimizeElastic(customers, depots, vehicles, 0, vehicleCapacity,
                           useSavings);
}

// Uses dedicated vehicles first, activates ad-hoc capacity when required, and
// leaves any remaining routes marked virtual rather than pretending they can dispatch.
vector<Route> RouteOptimizer::optimizeElastic(
    const vector<shared_ptr<Node>>& customers,
    const vector<shared_ptr<Node>>& depots, int dedicatedVehicles,
    int maxAdhocVehicles, int vehicleCapacity, bool useSavings) {
    if (dedicatedVehicles < 0 || maxAdhocVehicles < 0) {
        throw invalid_argument("Fleet counts cannot be negative.");
    }
    if (vehicleCapacity <= 0) {
        throw invalid_argument("Vehicle capacity must be greater than zero.");
    }
    if (!customers.empty() && depots.empty()) {
        throw invalid_argument("At least one depot is required to route customers.");
    }
    for (const auto& customer : customers) {
        if (!customer || customer->demand < 0)
            throw invalid_argument("Customers must have non-negative demand.");
        if (customer->demand > vehicleCapacity)
            throw invalid_argument("A customer demand exceeds the capacity of every vehicle.");
    }
    for (const auto& depot : depots)
        if (!depot) throw invalid_argument("Depot entries must not be null.");
    if (customers.empty()) return {};

    vehicleCount = dedicatedVehicles + maxAdhocVehicles;
    capacity = vehicleCapacity;
    clearCache();

    vector<Route> routes = useSavings
        ? savingsMultiDepot(customers, depots)
        : nearestMultiDepot(customers, depots);

    // Reduce route count using legal same-depot merges. If no legal merge can
    // satisfy the real fleet, preserve the remaining routes as virtual vehicles.
    constexpr double ADHOC_ACTIVATION_PENALTY = 25.0;
    const int dedicatedLimit = dedicatedVehicles;
    const int realFleetLimit = dedicatedVehicles + maxAdhocVehicles;
    while (static_cast<int>(routes.size()) > dedicatedLimit) {
        unique_ptr<MergeCandidate> best(findBestRouteMerge(routes, depots));
        if (!best) break;
        // Prefer paying the ad-hoc activation penalty over a very costly merge.
        // If the route count exceeds the real fleet, however, merging is mandatory.
        if (static_cast<int>(routes.size()) <= realFleetLimit &&
            best->addedScore > ADHOC_ACTIVATION_PENALTY) {
            break;
        }
        routes[best->firstIndex].stops = best->stops;
        routes.erase(routes.begin() + best->secondIndex);
    }

    for (auto& route : routes) {
        twoOpt(route);
        route.fleetTier = (&route - routes.data()) < dedicatedVehicles ? 0
            : ((&route - routes.data()) < dedicatedVehicles + maxAdhocVehicles ? 1 : 2);
    }
    return routes;
}

// Reverses route subsequences when doing so lowers the weighted travel score.
void RouteOptimizer::twoOpt(Route& route) {
    if (route.stops.size() < 2) {
        return;
    }

    bool improved = true;
    int iterations = 0;
    while (improved && iterations++ < 100) {
        improved = false;
        for (size_t i = 0; i < route.stops.size() - 1; ++i) {
            for (size_t k = i + 1; k < route.stops.size(); ++k) {
                const auto before =
                    (i == 0) ? route.depot : route.stops[i - 1];
                const auto first = route.stops[i];
                const auto last = route.stops[k];
                const auto after = (k == route.stops.size() - 1)
                                       ? route.depot
                                       : route.stops[k + 1];
                if (distance(before, last) + distance(first, after) +
                        TWO_OPT_EPSILON <
                    distance(before, first) + distance(last, after)) {
                    reverse(route.stops.begin() + i,
                            route.stops.begin() + k + 1);
                    improved = true;
                }
            }
        }
    }
}

// Accumulates a chosen leg measurement for a round trip through all stops.
double RouteOptimizer::routeMetric(
    const shared_ptr<Node>& depot, const vector<shared_ptr<Node>>& stops,
    Metric metric) {
    if (!depot || stops.empty()) {
        return 0.0;
    }

    double total = metricValue(leg(depot, stops[0]), metric);
    for (size_t i = 1; i < stops.size(); ++i) {
        total += metricValue(leg(stops[i - 1], stops[i]), metric);
    }
    total += metricValue(leg(stops.back(), depot), metric);
    return total;
}

// Selects one numeric measurement from a previously calculated leg.
double RouteOptimizer::metricValue(const Leg& legResult, Metric metric) const {
    if (metric == Metric::DISTANCE) {
        return legResult.km;
    }
    if (metric == Metric::TIME) {
        return legResult.minutes;
    }
    if (metric == Metric::COST) {
        return legResult.cost;
    }
    return legResult.score;
}

// Returns the weighted score of a depot-to-stops-to-depot trip.
double RouteOptimizer::routeDistance(
    const shared_ptr<Node>& depot, const vector<shared_ptr<Node>>& stops) {
    return routeMetric(depot, stops, Metric::SCORE);
}

// Returns the total round-trip distance in kilometers.
double RouteOptimizer::routeKm(
    const shared_ptr<Node>& depot, const vector<shared_ptr<Node>>& stops) {
    return routeMetric(depot, stops, Metric::DISTANCE);
}

// Returns the total round-trip travel time in minutes.
double RouteOptimizer::routeMinutes(
    const shared_ptr<Node>& depot, const vector<shared_ptr<Node>>& stops) {
    return routeMetric(depot, stops, Metric::TIME);
}

// Returns the total round-trip monetary cost.
double RouteOptimizer::routeCost(
    const shared_ptr<Node>& depot, const vector<shared_ptr<Node>>& stops) {
    return routeMetric(depot, stops, Metric::COST);
}

// Keeps the existing optimizer API while delegating graph work to its DSA module.
void RouteOptimizer::retainLargestStronglyConnectedComponent(
    set<shared_ptr<RoadNode>>& used) {
    RoadNetworkAlgorithms::retainLargestStronglyConnectedComponent(used);
}

// Applies the haversine formula to calculate great-circle distance in kilometers.
double RouteOptimizer::geoKm(
    double lat1, double lon1, double lat2, double lon2) {
    return RoadNetworkAlgorithms::geoKm(lat1, lon1, lat2, lon2);
}
