// File purpose: Assigns customers and constructs capacity- and fleet-limited routes.
// DSA part: Part 1 - Route construction.
#include "RouteOptimizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <utility>

using namespace std;

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
