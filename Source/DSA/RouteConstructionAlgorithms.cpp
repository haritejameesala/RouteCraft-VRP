// File purpose: Groups customers by depot and constructs capacity-limited routes.
// Flow:
//   Assign customers to depots -> build routes with nearest-neighbor or savings
//   -> merge feasible route ends -> return routes for local improvement.
// The custom DSA containers keep the algorithm independent of STL containers.
#include "RouteOptimizer.h"

#include <cmath>
#include <limits>
#include <utility>

namespace {

// Reverses the array in place without relying on a standard algorithm.
template <typename T>
void reverse(dsa::Array<T>& values) {
    for (std::size_t first = 0, last = values.size();
         first < last && first < --last; ++first) {
        T value = std::move(values[first]);
        values[first] = std::move(values[last]);
        values[last] = std::move(value);
    }
}

}  // namespace

// Remembers the customer pair and score used by the savings heuristic.
RouteOptimizer::Saving::Saving(std::shared_ptr<Node> first,
                               std::shared_ptr<Node> second, double value)
    : first(std::move(first)), second(std::move(second)), value(value) {}

// Stores the route indexes, merged stops, and extra score for a proposed join.
RouteOptimizer::MergeCandidate::MergeCandidate(
    int firstIndex, int secondIndex, std::shared_ptr<Node> depot,
    dsa::Array<std::shared_ptr<Node>> stops, double addedScore)
    : firstIndex(firstIndex),
      secondIndex(secondIndex),
      depot(std::move(depot)),
      stops(std::move(stops)),
      addedScore(addedScore) {}

// Keeps the best route merge found while reducing the active fleet.
dsa::HashMap<std::shared_ptr<Node>, dsa::Array<std::shared_ptr<Node>>>
RouteOptimizer::assignCustomersToDepots(
    const dsa::Array<std::shared_ptr<Node>>& customers,
    const dsa::Array<std::shared_ptr<Node>>& depots) {
    dsa::HashMap<std::shared_ptr<Node>, dsa::Array<std::shared_ptr<Node>>>
        groups;
    for (const auto& depot : depots) {
        groups[depot] = dsa::Array<std::shared_ptr<Node>>();
    }
    // Give each customer to its closest depot before building that depot's routes.
    for (const auto& customer : customers) {
        auto closestDepot = depots[0];
        for (const auto& depot : depots) {
            if (std::hypot(customer->x - depot->x, customer->y - depot->y) <
                std::hypot(customer->x - closestDepot->x,
                           customer->y - closestDepot->y)) {
                closestDepot = depot;
            }
        }
        groups[closestDepot].push_back(customer);
    }
    return groups;
}

// Builds each depot's routes by repeatedly taking its nearest feasible customer.
dsa::Array<Route> RouteOptimizer::nearestMultiDepot(
    const dsa::Array<std::shared_ptr<Node>>& customers,
    const dsa::Array<std::shared_ptr<Node>>& depots) {
    const auto groups = assignCustomersToDepots(customers, depots);
    dsa::Array<Route> routes;
    for (const auto& depot : depots) {
        auto remaining = groups.at(depot);
        while (!remaining.empty()) {
            Route route;
            route.depot = depot;
            auto current = depot;
            while (true) {
                std::shared_ptr<Node> nearest;
                double best = std::numeric_limits<double>::max();
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
                // Remove the served customer so it cannot be selected twice.
                for (std::size_t i = 0; i < remaining.size(); ++i) {
                    if (remaining[i] == nearest) {
                        remaining.erase(remaining.begin() + i);
                        break;
                    }
                }
                current = nearest;
            }
            if (!route.stops.empty()) {
                routes.push_back(std::move(route));
            }
        }
    }
    return routes;
}

// Starts with one-customer routes, then joins compatible endpoints by savings.
dsa::Array<Route> RouteOptimizer::savingsMultiDepot(
    const dsa::Array<std::shared_ptr<Node>>& customers,
    const dsa::Array<std::shared_ptr<Node>>& depots) {
    const auto groups = assignCustomersToDepots(customers, depots);
    dsa::Array<Route> routes;
    for (const auto& depot : depots) {
        const auto& group = groups.at(depot);
        dsa::Array<Route> depotRoutes;
        depotRoutes.reserve(group.size());
        dsa::HashMap<int, Route*> owner;
        for (const auto& customer : group) {
            Route route;
            route.depot = depot;
            route.stops.push_back(customer);
            depotRoutes.push_back(std::move(route));
            owner[customer->id] = &depotRoutes.back();
        }

        // Larger savings are considered first; insertion sort keeps this custom.
        dsa::Array<Saving> savings;
        for (std::size_t i = 0; i < group.size(); ++i) {
            for (std::size_t j = i + 1; j < group.size(); ++j) {
                const double value = distance(depot, group[i]) +
                                     distance(depot, group[j]) -
                                     distance(group[i], group[j]);
                savings.emplace_back(group[i], group[j], value);
            }
        }
        for (std::size_t i = 1; i < savings.size(); ++i) {
            Saving value = std::move(savings[i]);
            std::size_t j = i;
            while (j > 0 && savings[j - 1].value < value.value) {
                savings[j] = std::move(savings[j - 1]);
                --j;
            }
            savings[j] = std::move(value);
        }

        for (const auto& saving : savings) {
            // A merge is safe only when both customers still belong to separate,
            // capacity-feasible routes.
            Route* firstRoute = owner[saving.first->id];
            Route* secondRoute = owner[saving.second->id];
            if (!firstRoute || !secondRoute || firstRoute == secondRoute ||
                firstRoute->load() + secondRoute->load() > capacity) {
                continue;
            }
            auto mergedStops = mergeAtRouteEnds(*firstRoute, *secondRoute,
                                                saving);
            if (!mergedStops.empty()) {
                firstRoute->stops = std::move(mergedStops);
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
    return routes;
}

// Joins the two routes in an orientation that places the saved customers together.
dsa::Array<std::shared_ptr<Node>> RouteOptimizer::mergeAtRouteEnds(
    const Route& first, const Route& second, const Saving& saving) {
    const bool firstAtEnd = first.stops.back() == saving.first;
    const bool firstAtStart = first.stops.front() == saving.first;
    const bool secondAtEnd = second.stops.back() == saving.second;
    const bool secondAtStart = second.stops.front() == saving.second;

    dsa::Array<std::shared_ptr<Node>> merged;
    if (firstAtEnd && secondAtStart) {
        merged = first.stops;
        merged.insert(merged.end(), second.stops.begin(), second.stops.end());
    } else if (firstAtStart && secondAtEnd) {
        merged = second.stops;
        merged.insert(merged.end(), first.stops.begin(), first.stops.end());
    } else if (firstAtStart && secondAtStart) {
        merged = first.stops;
        reverse(merged);
        merged.insert(merged.end(), second.stops.begin(), second.stops.end());
    } else if (firstAtEnd && secondAtEnd) {
        merged = first.stops;
        auto reversedSecond = second.stops;
        reverse(reversedSecond);
        merged.insert(merged.end(), reversedSecond.begin(), reversedSecond.end());
    }
    return merged;
}

// Tries both directions for each route and keeps the cheapest legal merge.
RouteOptimizer::MergeCandidate* RouteOptimizer::findBestRouteMerge(
    const dsa::Array<Route>& routes) {
    double bestScore = std::numeric_limits<double>::max();
    MergeCandidate* bestCandidate = nullptr;
    for (std::size_t i = 0; i < routes.size(); ++i) {
        for (std::size_t j = i + 1; j < routes.size(); ++j) {
            if (routes[i].depot != routes[j].depot ||
                routes[i].load() + routes[j].load() > capacity) {
                continue;
            }
            for (int rotation = 0; rotation < 4; ++rotation) {
                auto firstStops = routes[i].stops;
                auto secondStops = routes[j].stops;
                if (rotation & 1) {
                    reverse(firstStops);
                }
                if (rotation & 2) {
                    reverse(secondStops);
                }
                firstStops.insert(firstStops.end(), secondStops.begin(),
                                  secondStops.end());
                const auto& depot = routes[i].depot;
                const double mergedScore = routeDistance(depot, firstStops);
                const double originalScore =
                    routeDistance(depot, routes[i].stops) +
                    routeDistance(depot, routes[j].stops);
                const double addedScore = mergedScore - originalScore;
                if (addedScore < bestScore) {
                    bestScore = addedScore;
                    delete bestCandidate;
                    bestCandidate = new MergeCandidate(
                        static_cast<int>(i), static_cast<int>(j), depot,
                        std::move(firstStops), addedScore);
                }
            }
        }
    }
    return bestCandidate;
}
