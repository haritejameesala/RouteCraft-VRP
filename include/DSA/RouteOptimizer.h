// File purpose: Declares the public route optimizer and its private algorithm helpers.
// Flow:
//   Accept customers, depots, fleet and objective settings -> construct and
//   improve routes -> expose route metrics and road-leg lookup to callers.
#ifndef ROUTE_OPTIMIZER_H
#define ROUTE_OPTIMIZER_H

#include "RouteObjects.h"

#include <memory>
#include <string>

class RouteOptimizer {
public:
    // Keeps a reference to the caller-owned road graph.
    explicit RouteOptimizer(
        dsa::HashMap<long long, std::shared_ptr<RoadNode>>& roadNodes);

    // Updates score weights and invalidates cached leg calculations.
    void setWeights(double distance, double time, double cost);
    // Clears stale leg measurements after the graph or routing settings change.
    void clearCache();
    // Selects whether legs should be found in the road graph or estimated directly.
    void setRoadNetworkLoaded(bool loaded);

    // Builds and improves routes using the selected construction heuristic.
    dsa::Array<Route> optimize(
        const dsa::Array<std::shared_ptr<Node>>& customers,
        const dsa::Array<std::shared_ptr<Node>>& depots, int vehicleCount,
        int capacity, bool useSavings);
    // Activates dedicated, then ad-hoc vehicles; marks remaining routes virtual.
    dsa::Array<Route> optimizeElastic(
        const dsa::Array<std::shared_ptr<Node>>& customers,
        const dsa::Array<std::shared_ptr<Node>>& depots,
        int dedicatedVehicles, int maxAdhocVehicles, int capacity,
        bool useSavings);
    // Returns a round-trip measurement or score for a route.
    double routeDistance(const std::shared_ptr<Node>& depot,
                         const dsa::Array<std::shared_ptr<Node>>& stops);
    // Returns the route's total road distance in kilometres.
    double routeKm(const std::shared_ptr<Node>& depot,
                   const dsa::Array<std::shared_ptr<Node>>& stops);
    // Returns the route's total estimated travel time.
    double routeMinutes(const std::shared_ptr<Node>& depot,
                        const dsa::Array<std::shared_ptr<Node>>& stops);
    // Returns the route's total estimated operating cost.
    double routeCost(const std::shared_ptr<Node>& depot,
                     const dsa::Array<std::shared_ptr<Node>>& stops);
    // Finds and caches the best available leg between two locations.
    Leg leg(const std::shared_ptr<Node>& from,
            const std::shared_ptr<Node>& to);
    // Finds the closest road vertex to projected coordinates.
    std::shared_ptr<RoadNode> nearestRoadNode(double x, double y);

    // Retains the largest strongly connected road-graph component.
    static void retainLargestStronglyConnectedComponent(
        dsa::UniqueSet<std::shared_ptr<RoadNode>>& used);
    // Calculates great-circle distance between two coordinate pairs.
    static double geoKm(double lat1, double lon1, double lat2, double lon2);

private:
    // Selects which leg measurement shortest-path search should minimize.
    enum class Metric { DISTANCE, TIME, COST, SCORE };

    // Captures one customer pair and the score saved by joining their routes.
    struct Saving {
        std::shared_ptr<Node> first;
        std::shared_ptr<Node> second;
        double value = 0.0;

        // Keeps a savings record empty until its customer pair is selected.
        Saving() = default;
        Saving(std::shared_ptr<Node> first, std::shared_ptr<Node> second,
               double value);
    };

    // Holds a feasible route merge while the optimizer compares candidates.
    struct MergeCandidate {
        int firstIndex = 0;
        int secondIndex = 0;
        std::shared_ptr<Node> depot;
        dsa::Array<std::shared_ptr<Node>> stops;
        double addedScore = 0.0;

        // Keeps an unused candidate in a safe, empty state.
        MergeCandidate() = default;
        MergeCandidate(int firstIndex, int secondIndex,
                       std::shared_ptr<Node> depot,
                       dsa::Array<std::shared_ptr<Node>> stops,
                       double addedScore);
    };

    // These are non-owning references to graph data supplied by the application.
    dsa::HashMap<long long, std::shared_ptr<RoadNode>>& roadNodes;
    // Stores reusable leg measurements and the settings used to score them.
    dsa::HashMap<std::string, Leg> legCache;
    double weightDistance = 0.40;
    double weightTime = 0.35;
    double weightCost = 0.25;
    bool roadNetworkLoaded = false;
    int vehicleCount = 0;
    int capacity = 0;

    // Returns the active objective score for a direct or road-network leg.
    double distance(const std::shared_ptr<Node>& first,
                    const std::shared_ptr<Node>& second);
    // Combines the three leg measurements using the current normalized weights.
    double objectiveScore(double km, double minutes, double cost) const;
    // Accumulates a chosen leg measurement around the complete closed route.
    double routeMetric(const std::shared_ptr<Node>& depot,
                       const dsa::Array<std::shared_ptr<Node>>& stops,
                       Metric metric);
    // Extracts the selected measurement from an already calculated leg.
    double metricValue(const Leg& leg, Metric metric) const;
    // Groups customers with the depot that gives the nearest assignment score.
    dsa::HashMap<std::shared_ptr<Node>, dsa::Array<std::shared_ptr<Node>>>
    assignCustomersToDepots(
        const dsa::Array<std::shared_ptr<Node>>& customers,
        const dsa::Array<std::shared_ptr<Node>>& depots);
    // Builds capacity-feasible routes by repeatedly selecting the nearest stop.
    dsa::Array<Route> nearestMultiDepot(
        const dsa::Array<std::shared_ptr<Node>>& customers,
        const dsa::Array<std::shared_ptr<Node>>& depots);
    // Builds single-customer routes, then joins compatible ends by savings.
    dsa::Array<Route> savingsMultiDepot(
        const dsa::Array<std::shared_ptr<Node>>& customers,
        const dsa::Array<std::shared_ptr<Node>>& depots);
    // Joins two routes at the endpoints selected by one savings pair.
    dsa::Array<std::shared_ptr<Node>> mergeAtRouteEnds(
        const Route& first, const Route& second, const Saving& saving);
    // Finds the legal route merge with the smallest additional objective score.
    MergeCandidate* findBestRouteMerge(const dsa::Array<Route>& routes);
    // Reverses route segments while the change shortens the weighted objective.
    void twoOpt(Route& route);
};

#endif
