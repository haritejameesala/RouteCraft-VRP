#include "RouteOptimizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <stack>
#include <stdexcept>
#include <string>
#include <utility>

using namespace std;

namespace {

constexpr double TWO_OPT_EPSILON = 1e-9;
constexpr double THREE_OPT_EPSILON = 1e-8;

}  // namespace

// Keeps the customer pair and calculated savings value together.
RouteOptimizer::Saving::Saving(
    shared_ptr<Node> first, shared_ptr<Node> second, double value)
    : first(move(first)), second(move(second)), value(value) {}

// Captures the route indexes, chosen depot, and stop order for a merge.
RouteOptimizer::MergeCandidate::MergeCandidate(
    int firstIndex, int secondIndex, shared_ptr<Node> depot,
    vector<shared_ptr<Node>> stops)
    : firstIndex(firstIndex),
      secondIndex(secondIndex),
      depot(move(depot)),
      stops(move(stops)) {}

// Begins an iterative graph traversal at the supplied road node.
RouteOptimizer::TraversalFrame::TraversalFrame(shared_ptr<RoadNode> node)
    : node(move(node)), nextEdge(0) {}

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
    if (vehicles <= 0) {
        throw invalid_argument("Vehicle count must be greater than zero.");
    }
    if (vehicleCapacity <= 0) {
        throw invalid_argument("Vehicle capacity must be greater than zero.");
    }
    if (!customers.empty() && depots.empty()) {
        throw invalid_argument(
            "At least one depot is required to route customers.");
    }
    for (const auto& customer : customers) {
        if (!customer || customer->demand < 0) {
            throw invalid_argument(
                "Customers must have non-negative demand.");
        }
    }
    for (const auto& depot : depots) {
        if (!depot) {
            throw invalid_argument("Depot entries must not be null.");
        }
    }
    if (customers.empty()) {
        return {};
    }

    vehicleCount = vehicles;
    capacity = vehicleCapacity;
    clearCache();

    vector<Route> routes = useSavings
                               ? savingsMultiDepot(customers, depots)
                               : nearestMultiDepot(customers, depots);
    for (auto& route : routes) {
        twoOpt(route);
        threeOptLite(route);
    }
    routes.erase(
        remove_if(routes.begin(), routes.end(),
                  [](const Route& route) { return route.stops.empty(); }),
        routes.end());
    return routes;
}

// Looks up a cached leg or calculates it from the road graph or straight-line fallback.
Leg RouteOptimizer::leg(
    const shared_ptr<Node>& from, const shared_ptr<Node>& to) {
    const string key =
        to_string(from->id) + ":" + (from->isDepot ? "D" : "C") + ":" +
        to_string(to->id) + ":" + (to->isDepot ? "D" : "C");
    const auto cached = legCache.find(key);
    if (cached != legCache.end()) {
        return cached->second;
    }

    Leg result;
    if (roadNetworkLoaded && !roadNodes.empty()) {
        const auto start = nearestRoadNode(from->x, from->y);
        const auto end = nearestRoadNode(to->x, to->y);
        if (start && end) {
            result = shortestLeg(start, end);
            if (!result.path.empty()) {
                legCache[key] = result;
                return result;
            }
        }
    }

    const double straightLine =
        hypot(from->x - to->x, from->y - to->y);
    result.km = straightLine;
    result.minutes = straightLine * 2.0;
    result.cost = straightLine * 12.0;
    result.score = objectiveScore(result.km, result.minutes, result.cost);
    legCache[key] = result;
    return result;
}

// Scans the loaded road graph and returns the node nearest the supplied point.
shared_ptr<RoadNode> RouteOptimizer::nearestRoadNode(double x, double y) {
    shared_ptr<RoadNode> nearest = nullptr;
    double best = numeric_limits<double>::max();
    for (const auto& entry : roadNodes) {
        const auto& node = entry.second;
        const double currentDistance = hypot(node->x - x, node->y - y);
        if (currentDistance < best) {
            best = currentDistance;
            nearest = node;
        }
    }
    return nearest;
}

// Assigns each customer to the geographically closest depot.
unordered_map<shared_ptr<Node>, vector<shared_ptr<Node>>>
RouteOptimizer::assignCustomersToDepots(
    const vector<shared_ptr<Node>>& customers,
    const vector<shared_ptr<Node>>& depots) {
    unordered_map<shared_ptr<Node>, vector<shared_ptr<Node>>> groups;
    for (const auto& depot : depots) {
        groups[depot] = vector<shared_ptr<Node>>();
    }

    for (const auto& customer : customers) {
        auto closestDepot = depots[0];
        for (const auto& depot : depots) {
            if (hypot(customer->x - depot->x, customer->y - depot->y) <
                hypot(customer->x - closestDepot->x,
                      customer->y - closestDepot->y)) {
                closestDepot = depot;
            }
        }
        groups[closestDepot].push_back(customer);
    }
    return groups;
}

// Builds each route by repeatedly visiting the nearest remaining feasible customer.
vector<Route> RouteOptimizer::nearestMultiDepot(
    const vector<shared_ptr<Node>>& customers,
    const vector<shared_ptr<Node>>& depots) {
    const auto groups = assignCustomersToDepots(customers, depots);
    vector<Route> routes;
    for (const auto& depot : depots) {
        auto remaining = groups.at(depot);
        while (!remaining.empty() &&
               static_cast<int>(routes.size()) < vehicleCount) {
            Route route;
            route.depot = depot;
            auto current = depot;

            while (true) {
                shared_ptr<Node> nearest = nullptr;
                double best = numeric_limits<double>::max();
                for (const auto& customer : remaining) {
                    if (route.load() + customer->demand <= capacity &&
                        distance(current, customer) < best) {
                        nearest = customer;
                        best = distance(current, customer);
                    }
                }
                if (!nearest) {
                    break;
                }

                route.stops.push_back(nearest);
                remaining.erase(
                    remove(remaining.begin(), remaining.end(), nearest),
                    remaining.end());
                current = nearest;
            }
            if (!route.stops.empty()) {
                routes.push_back(route);
            }
        }
    }
    return routes;
}

vector<Route> RouteOptimizer::savingsMultiDepot(
    const vector<shared_ptr<Node>>& customers,
    const vector<shared_ptr<Node>>& depots) {
    const auto groups = assignCustomersToDepots(customers, depots);
    vector<Route> routes;

    // Starts with single-customer routes, merges the highest-savings pairs,
    // then reduces the result to the available fleet size when feasible.
    for (const auto& depot : depots) {
        const auto& group = groups.at(depot);
        vector<Route> depotRoutes;
        depotRoutes.reserve(group.size());
        unordered_map<int, Route*> owner;

        for (const auto& customer : group) {
            Route route;
            route.depot = depot;
            route.stops.push_back(customer);
            depotRoutes.push_back(route);
            owner[customer->id] = &depotRoutes.back();
        }

        vector<Saving> savings;
        for (size_t i = 0; i < group.size(); ++i) {
            for (size_t j = i + 1; j < group.size(); ++j) {
                const double value = distance(depot, group[i]) +
                                     distance(depot, group[j]) -
                                     distance(group[i], group[j]);
                savings.emplace_back(group[i], group[j], value);
            }
        }
        sort(savings.begin(), savings.end(),
             [](const Saving& first, const Saving& second) {
                 return first.value > second.value;
             });

        for (const auto& saving : savings) {
            Route* firstRoute = owner[saving.first->id];
            Route* secondRoute = owner[saving.second->id];
            if (!firstRoute || !secondRoute || firstRoute == secondRoute ||
                firstRoute->load() + secondRoute->load() > capacity) {
                continue;
            }

            auto mergedStops = mergeAtRouteEnds(
                *firstRoute, *secondRoute, saving);
            if (!mergedStops.empty()) {
                firstRoute->stops = mergedStops;
                for (const auto& stop : secondRoute->stops) {
                    owner[stop->id] = firstRoute;
                }
                secondRoute->stops.clear();
            }
        }

        for (auto& route : depotRoutes) {
            if (!route.stops.empty()) {
                routes.push_back(route);
            }
        }
    }

    while (static_cast<int>(routes.size()) > vehicleCount) {
        unique_ptr<MergeCandidate> best(findBestRouteMerge(routes, depots));
        if (!best) {
            break;
        }
        routes[best->firstIndex].stops = best->stops;
        routes[best->firstIndex].depot = best->depot;
        routes.erase(routes.begin() + best->secondIndex);
    }
    return routes;
}

// Joins two routes in an endpoint-compatible order without dropping customers.
vector<shared_ptr<Node>> RouteOptimizer::mergeAtRouteEnds(
    const Route& first, const Route& second, const Saving& saving) {
    const auto firstSaved = saving.first;
    const auto secondSaved = saving.second;
    const bool firstAtEnd = first.stops.back() == firstSaved;
    const bool firstAtStart = first.stops.front() == firstSaved;
    const bool secondAtEnd = second.stops.back() == secondSaved;
    const bool secondAtStart = second.stops.front() == secondSaved;

    vector<shared_ptr<Node>> merged;
    if (firstAtEnd && secondAtStart) {
        merged = first.stops;
        merged.insert(merged.end(), second.stops.begin(), second.stops.end());
    } else if (firstAtStart && secondAtEnd) {
        merged = second.stops;
        merged.insert(merged.end(), first.stops.begin(), first.stops.end());
    } else if (firstAtStart && secondAtStart) {
        merged = first.stops;
        reverse(merged.begin(), merged.end());
        merged.insert(merged.end(), second.stops.begin(), second.stops.end());
    } else if (firstAtEnd && secondAtEnd) {
        merged = first.stops;
        auto reversedSecond = second.stops;
        reverse(reversedSecond.begin(), reversedSecond.end());
        merged.insert(merged.end(), reversedSecond.begin(),
                      reversedSecond.end());
    }
    return merged;
}

// Evaluates legal route orientations and depots, returning the best merge.
RouteOptimizer::MergeCandidate* RouteOptimizer::findBestRouteMerge(
    const vector<Route>& routes, const vector<shared_ptr<Node>>& depots) {
    double bestScore = numeric_limits<double>::max();
    MergeCandidate* bestCandidate = nullptr;

    for (size_t i = 0; i < routes.size(); ++i) {
        for (size_t j = i + 1; j < routes.size(); ++j) {
            if (routes[i].load() + routes[j].load() > capacity) {
                continue;
            }
            for (int rotation = 0; rotation < 4; ++rotation) {
                auto firstStops = routes[i].stops;
                auto secondStops = routes[j].stops;
                if (rotation & 1) {
                    reverse(firstStops.begin(), firstStops.end());
                }
                if (rotation & 2) {
                    reverse(secondStops.begin(), secondStops.end());
                }
                firstStops.insert(firstStops.end(), secondStops.begin(),
                                  secondStops.end());

                for (const auto& depot : depots) {
                    const double score = routeDistance(depot, firstStops);
                    if (score < bestScore) {
                        bestScore = score;
                        delete bestCandidate;
                        bestCandidate = new MergeCandidate(
                            static_cast<int>(i), static_cast<int>(j), depot,
                            firstStops);
                    }
                }
            }
        }
    }
    return bestCandidate;
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

// Tests a limited set of segment reversals and applies the first improving result.
void RouteOptimizer::threeOptLite(Route& route) {
    const int stopCount = static_cast<int>(route.stops.size());
    if (stopCount < 5) {
        return;
    }

    bool improved = true;
    int rounds = 0;
    while (improved && rounds++ < 2) {
        improved = false;
        for (int i = 0; i < stopCount - 3; ++i) {
            for (int j = i + 1; j < stopCount - 2; ++j) {
                for (int k = j + 1; k < stopCount - 1; ++k) {
                    const auto original = route.stops;
                    auto best = original;
                    double bestDistance = routeDistance(route.depot, original);

                    for (const auto& range :
                         {pair<int, int>{i, j}, {j, k}, {i, k}}) {
                        auto candidate = original;
                        reverse(candidate.begin() + range.first,
                                candidate.begin() + range.second);
                        const double candidateDistance =
                            routeDistance(route.depot, candidate);
                        if (candidateDistance + THREE_OPT_EPSILON <
                            bestDistance) {
                            bestDistance = candidateDistance;
                            best = move(candidate);
                        }
                    }
                    if (best != original) {
                        route.stops = move(best);
                        improved = true;
                        return;
                    }
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

// Finds a lowest-score graph path and totals its distance, time, and cost.
Leg RouteOptimizer::shortestLeg(
    const shared_ptr<RoadNode>& start, const shared_ptr<RoadNode>& target) {
    Leg result;
    if (start == target) {
        result.path.push_back(start);
        return result;
    }

    unordered_map<shared_ptr<RoadNode>, double> distances;
    unordered_map<shared_ptr<RoadNode>, shared_ptr<RoadNode>> previous;
    auto compare = [&](const shared_ptr<RoadNode>& first,
                       const shared_ptr<RoadNode>& second) {
        return distances[first] > distances[second];
    };
    priority_queue<shared_ptr<RoadNode>, vector<shared_ptr<RoadNode>>,
                   decltype(compare)>
        pending(compare);
    set<shared_ptr<RoadNode>> settled;
    distances[start] = 0.0;
    pending.push(start);

    // Dijkstra's algorithm uses the weighted score as its edge cost.
    while (!pending.empty()) {
        const auto current = pending.top();
        pending.pop();
        if (!settled.insert(current).second) {
            continue;
        }
        if (current == target) {
            break;
        }

        for (const auto& edge : current->edges) {
            const auto next = edge.to.lock();
            if (!next) {
                continue;
            }
            const double candidateDistance =
                distances[current] +
                objectiveScore(edge.km, edge.minutes, edge.cost);
            if (distances.find(next) == distances.end() ||
                candidateDistance < distances[next]) {
                distances[next] = candidateDistance;
                previous[next] = current;
                pending.push(next);
            }
        }
    }

    if (distances.find(target) == distances.end()) {
        result.km = geoKm(start->lat, start->lon, target->lat, target->lon);
        result.minutes = result.km * 2.0;
        result.cost = result.km * 12.0;
        result.score =
            objectiveScore(result.km, result.minutes, result.cost);
        result.path = {start, target};
        return result;
    }

    auto current = target;
    while (current) {
        result.path.push_back(current);
        if (current == start) {
            break;
        }
        current = previous[current];
    }
    reverse(result.path.begin(), result.path.end());

    for (size_t i = 0; i < result.path.size() - 1; ++i) {
        for (const auto& edge : result.path[i]->edges) {
            if (edge.to.lock() == result.path[i + 1]) {
                result.km += edge.km;
                result.minutes += edge.minutes;
                result.cost += edge.cost;
                break;
            }
        }
    }
    result.score = objectiveScore(result.km, result.minutes, result.cost);
    return result;
}

// Finds the largest strongly connected road-graph component and prunes the rest.
void RouteOptimizer::retainLargestStronglyConnectedComponent(
    set<shared_ptr<RoadNode>>& used) {
    vector<shared_ptr<RoadNode>> nodes(used.begin(), used.end());
    unordered_map<shared_ptr<RoadNode>, vector<shared_ptr<RoadNode>>>
        reverseEdges;
    for (const auto& node : nodes) {
        for (const auto& edge : node->edges) {
            if (auto target = edge.to.lock()) {
                reverseEdges[target].push_back(node);
            }
        }
    }

    // First pass of Kosaraju's algorithm records reverse finishing order.
    set<shared_ptr<RoadNode>> visited;
    stack<shared_ptr<RoadNode>> finishOrder;
    for (const auto& root : nodes) {
        if (!visited.insert(root).second) {
            continue;
        }
        stack<TraversalFrame> traversal;
        traversal.emplace(root);
        while (!traversal.empty()) {
            auto& frame = traversal.top();
            if (frame.nextEdge < frame.node->edges.size()) {
                auto next = frame.node->edges[frame.nextEdge++].to.lock();
                if (next && used.count(next) && visited.insert(next).second) {
                    traversal.emplace(next);
                }
            } else {
                finishOrder.push(frame.node);
                traversal.pop();
            }
        }
    }

    // The second pass collects components over the reversed graph.
    set<shared_ptr<RoadNode>> assigned;
    set<shared_ptr<RoadNode>> largestComponent;
    while (!finishOrder.empty()) {
        const auto root = finishOrder.top();
        finishOrder.pop();
        if (!assigned.insert(root).second) {
            continue;
        }

        set<shared_ptr<RoadNode>> component;
        stack<shared_ptr<RoadNode>> traversal;
        traversal.push(root);
        while (!traversal.empty()) {
            const auto node = traversal.top();
            traversal.pop();
            component.insert(node);
            for (const auto& predecessor : reverseEdges[node]) {
                if (assigned.insert(predecessor).second) {
                    traversal.push(predecessor);
                }
            }
        }
        if (component.size() > largestComponent.size()) {
            largestComponent = component;
        }
    }

    for (auto it = used.begin(); it != used.end();) {
        if (!largestComponent.count(*it)) {
            it = used.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& node : used) {
        node->edges.erase(
            remove_if(node->edges.begin(), node->edges.end(),
                      [&](const RoadEdge& edge) {
                          const auto target = edge.to.lock();
                          return !target || !used.count(target);
                      }),
            node->edges.end());
    }
}

// Applies the haversine formula to calculate great-circle distance in kilometers.
double RouteOptimizer::geoKm(
    double lat1, double lon1, double lat2, double lon2) {
    constexpr double pi = 3.14159265358979323846;
    constexpr double earthRadiusKm = 6371.0;
    const double deltaLatitude = (lat2 - lat1) * pi / 180.0;
    const double deltaLongitude = (lon2 - lon1) * pi / 180.0;
    const double a =
        sin(deltaLatitude / 2) * sin(deltaLatitude / 2) +
        cos(lat1 * pi / 180.0) * cos(lat2 * pi / 180.0) *
            sin(deltaLongitude / 2) * sin(deltaLongitude / 2);
    return 2 * earthRadiusKm * asin(sqrt(a));
}
