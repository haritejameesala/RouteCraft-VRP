// File purpose: Controls route optimization, scoring, fleet assignment, and 2-opt.
// Flow:
//   Validate input -> construct routes -> fit routes to dedicated/ad-hoc/virtual
//   fleet tiers -> improve stop order -> calculate route and leg measurements.
// Cached legs are cleared whenever weights or road-network availability changes.

#include "RouteOptimizer.h"

#include "RoadNetworkAlgorithms.h"

#include <stdexcept>

namespace {

constexpr double TWO_OPT_EPSILON = 1e-9;

}  // namespace

// The graph remains owned by the caller and is shared with the optimizer.
RouteOptimizer::RouteOptimizer(
    dsa::HashMap<long long, std::shared_ptr<RoadNode>>& roadNodes)
    : roadNodes(roadNodes) {}

// New weights change route scores, so old cached legs must be recalculated.
void RouteOptimizer::setWeights(double distance, double time, double cost) {
    weightDistance = distance;
    weightTime = time;
    weightCost = cost;
    clearCache();
}

// Discards measurements computed with earlier routing inputs or weights.
void RouteOptimizer::clearCache() {
    legCache.clear();
}

// Switches between graph routing and straight-line fallback measurements.
void RouteOptimizer::setRoadNetworkLoaded(bool loaded) {
    roadNetworkLoaded = loaded;
    clearCache();
}

// Uses the weighted score of the leg when comparing candidate stops.
double RouteOptimizer::distance(const std::shared_ptr<Node>& first,
                                const std::shared_ptr<Node>& second) {
    return leg(first, second).score;
}

// Puts kilometres, time, and cost onto one comparable weighted score.
double RouteOptimizer::objectiveScore(double km, double minutes,
                                      double cost) const {
    return weightDistance * km + weightTime * (minutes / 60.0 * 30.0) +
           weightCost * (cost / 12.0);
}

// Keeps the simpler fleet API by calling the elastic-fleet implementation.
dsa::Array<Route> RouteOptimizer::optimize(
    const dsa::Array<std::shared_ptr<Node>>& customers,
    const dsa::Array<std::shared_ptr<Node>>& depots, int vehicles,
    int vehicleCapacity, bool useSavings) {
    return optimizeElastic(customers, depots, vehicles, 0, vehicleCapacity,
                           useSavings);
}

// Validates the request, constructs routes, then assigns each a fleet tier.
dsa::Array<Route> RouteOptimizer::optimizeElastic(
    const dsa::Array<std::shared_ptr<Node>>& customers,
    const dsa::Array<std::shared_ptr<Node>>& depots, int dedicatedVehicles,
    int maxAdhocVehicles, int vehicleCapacity, bool useSavings) {
    if (dedicatedVehicles < 0 || maxAdhocVehicles < 0) {
        throw std::invalid_argument("Fleet counts cannot be negative.");
    }
    if (vehicleCapacity <= 0) {
        throw std::invalid_argument("Vehicle capacity must be greater than zero.");
    }
    if (!customers.empty() && depots.empty()) {
        throw std::invalid_argument(
            "At least one depot is required to route customers.");
    }
    for (const auto& customer : customers) {
        if (!customer || customer->demand < 0) {
            throw std::invalid_argument(
                "Customers must have non-negative demand.");
        }
        if (customer->demand > vehicleCapacity) {
            throw std::invalid_argument(
                "A customer demand exceeds the capacity of every vehicle.");
        }
    }
    for (const auto& depot : depots) {
        if (!depot) {
            throw std::invalid_argument("Depot entries must not be null.");
        }
    }
    if (customers.empty()) {
        return {};
    }

    vehicleCount = dedicatedVehicles + maxAdhocVehicles;
    capacity = vehicleCapacity;
    clearCache();

    // Pick the requested construction method before considering fleet limits.
    dsa::Array<Route> routes = useSavings
                                   ? savingsMultiDepot(customers, depots)
                                   : nearestMultiDepot(customers, depots);

    constexpr double adhocActivationPenalty = 25.0;
    const int dedicatedLimit = dedicatedVehicles;
    const int realFleetLimit = dedicatedVehicles + maxAdhocVehicles;
    // Merge routes only when the score trade-off is preferable to ad-hoc use.
    while (static_cast<int>(routes.size()) > dedicatedLimit) {
        MergeCandidate* best = findBestRouteMerge(routes);
        if (best == nullptr) {
            break;
        }
        if (static_cast<int>(routes.size()) <= realFleetLimit &&
            best->addedScore > adhocActivationPenalty) {
            delete best;
            break;
        }
        routes[best->firstIndex].stops = best->stops;
        routes.erase(routes.begin() + best->secondIndex);
        delete best;
    }

    for (std::size_t i = 0; i < routes.size(); ++i) {
        twoOpt(routes[i]);
        routes[i].fleetTier =
            static_cast<int>(i) < dedicatedVehicles
                ? 0
                : (static_cast<int>(i) < dedicatedVehicles + maxAdhocVehicles
                       ? 1
                       : 2);
    }
    return routes;
}

// Reverses improving stop ranges until the route settles or the safety limit hits.
void RouteOptimizer::twoOpt(Route& route) {
    if (route.stops.size() < 2) {
        return;
    }
    bool improved = true;
    int iterations = 0;
    while (improved && iterations++ < 100) {
        improved = false;
        for (std::size_t i = 0; i + 1 < route.stops.size(); ++i) {
            for (std::size_t k = i + 1; k < route.stops.size(); ++k) {
                const auto before = i == 0 ? route.depot : route.stops[i - 1];
                const auto first = route.stops[i];
                const auto last = route.stops[k];
                const auto after = k == route.stops.size() - 1
                                       ? route.depot
                                       : route.stops[k + 1];
                if (distance(before, last) + distance(first, after) +
                        TWO_OPT_EPSILON <
                    distance(before, first) + distance(last, after)) {
                    for (std::size_t left = i, right = k; left < right;
                         ++left, --right) {
                        auto value = std::move(route.stops[left]);
                        route.stops[left] = std::move(route.stops[right]);
                        route.stops[right] = std::move(value);
                    }
                    improved = true;
                }
            }
        }
    }
}

// Adds one selected leg metric for every segment of the closed route.
double RouteOptimizer::routeMetric(
    const std::shared_ptr<Node>& depot,
    const dsa::Array<std::shared_ptr<Node>>& stops, Metric metric) {
    if (!depot || stops.empty()) {
        return 0.0;
    }
    double total = metricValue(leg(depot, stops[0]), metric);
    for (std::size_t i = 1; i < stops.size(); ++i) {
        total += metricValue(leg(stops[i - 1], stops[i]), metric);
    }
    total += metricValue(leg(stops.back(), depot), metric);
    return total;
}

// Reads the requested measure from an already calculated leg.
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

// Returns the weighted score for the depot-to-stops-to-depot trip.
double RouteOptimizer::routeDistance(
    const std::shared_ptr<Node>& depot,
    const dsa::Array<std::shared_ptr<Node>>& stops) {
    return routeMetric(depot, stops, Metric::SCORE);
}

// Returns the total road or straight-line distance for the round trip.
double RouteOptimizer::routeKm(
    const std::shared_ptr<Node>& depot,
    const dsa::Array<std::shared_ptr<Node>>& stops) {
    return routeMetric(depot, stops, Metric::DISTANCE);
}

// Returns the total estimated travel time for the round trip.
double RouteOptimizer::routeMinutes(
    const std::shared_ptr<Node>& depot,
    const dsa::Array<std::shared_ptr<Node>>& stops) {
    return routeMetric(depot, stops, Metric::TIME);
}

// Returns the total estimated cost for the round trip.
double RouteOptimizer::routeCost(
    const std::shared_ptr<Node>& depot,
    const dsa::Array<std::shared_ptr<Node>>& stops) {
    return routeMetric(depot, stops, Metric::COST);
}
