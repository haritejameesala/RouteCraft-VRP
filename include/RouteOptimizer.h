#ifndef ROUTE_OPTIMIZER_H
#define ROUTE_OPTIMIZER_H

#include "RouteObjects.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Builds capacity-constrained vehicle routes and measures travel over a road graph.
// Example: RouteOptimizer optimizer(roadNodes); auto routes =
// optimizer.optimize(customers, depots, 2, 10, true);
class RouteOptimizer {
public:
    // Uses the caller-owned road graph so loaded graph nodes can be shared.
    explicit RouteOptimizer(
        std::unordered_map<long long, std::shared_ptr<RoadNode>>& roadNodes);

    // Changes the distance/time/cost weights and invalidates cached leg scores.
    void setWeights(double distance, double time, double cost);
    // Discards cached leg calculations after routing inputs have changed.
    void clearCache();
    // Enables or disables road-graph routing and invalidates cached legs.
    void setRoadNetworkLoaded(bool loaded);

    // Creates routes using nearest-neighbor or savings construction, then improves them.
    std::vector<Route> optimize(
        const std::vector<std::shared_ptr<Node>>& customers,
        const std::vector<std::shared_ptr<Node>>& depots, int vehicleCount,
        int capacity, bool useSavings);
    // Returns a route's total weighted optimization score.
    double routeDistance(const std::shared_ptr<Node>& depot,
                         const std::vector<std::shared_ptr<Node>>& stops);
    // Returns round-trip route distance in kilometers.
    double routeKm(const std::shared_ptr<Node>& depot,
                   const std::vector<std::shared_ptr<Node>>& stops);
    // Returns round-trip route travel time in minutes.
    double routeMinutes(const std::shared_ptr<Node>& depot,
                       const std::vector<std::shared_ptr<Node>>& stops);
    // Returns round-trip route monetary cost.
    double routeCost(const std::shared_ptr<Node>& depot,
                     const std::vector<std::shared_ptr<Node>>& stops);
    // Finds and caches the best available leg between two locations.
    Leg leg(const std::shared_ptr<Node>& from,
            const std::shared_ptr<Node>& to);
    // Finds the graph vertex closest to the supplied projected coordinates.
    std::shared_ptr<RoadNode> nearestRoadNode(double x, double y);

    // Keeps the largest strongly connected graph component and removes stray edges.
    static void retainLargestStronglyConnectedComponent(
        std::set<std::shared_ptr<RoadNode>>& used);
    // Calculates great-circle distance between two latitude/longitude points.
    static double geoKm(double lat1, double lon1, double lat2, double lon2);

private:
    // Selects which leg measurement routeMetric should accumulate.
    enum class Metric { DISTANCE, TIME, COST, SCORE };

    // Stores a pair of customers and the benefit of joining their routes.
    struct Saving {
        std::shared_ptr<Node> first;   // First customer in the candidate pair.
        std::shared_ptr<Node> second;  // Second customer in the candidate pair.
        double value;                  // Score improvement from joining them.

        Saving(std::shared_ptr<Node> first, std::shared_ptr<Node> second,
               double value);
    };

    // Describes the best route merge found while reducing the fleet size.
    struct MergeCandidate {
        int firstIndex;  // Route index that will hold the merged route.
        int secondIndex; // Route index removed after a successful merge.
        std::shared_ptr<Node> depot;  // Depot chosen for the merged route.
        std::vector<std::shared_ptr<Node>> stops;  // Merged visit order.

        MergeCandidate(int firstIndex, int secondIndex,
                       std::shared_ptr<Node> depot,
                       std::vector<std::shared_ptr<Node>> stops);
    };

    // Holds a graph vertex and next-edge index for iterative depth-first search.
    struct TraversalFrame {
        std::shared_ptr<RoadNode> node;  // Vertex currently being traversed.
        std::size_t nextEdge;            // Next outgoing edge to inspect.

        explicit TraversalFrame(std::shared_ptr<RoadNode> node);
    };

    std::unordered_map<long long, std::shared_ptr<RoadNode>>& roadNodes;  // Input road graph.
    std::unordered_map<std::string, Leg> legCache;                        // Reused leg results.
    double weightDistance = 0.40;  // Contribution of distance to the score.
    double weightTime = 0.35;      // Contribution of travel time to the score.
    double weightCost = 0.25;      // Contribution of monetary cost to the score.
    bool roadNetworkLoaded = false;  // Whether legs should use the road graph.
    int vehicleCount = 0;             // Fleet limit for the active optimization.
    int capacity = 0;                 // Per-vehicle demand limit.

    // Returns the weighted score of a single leg.
    double distance(const std::shared_ptr<Node>& first,
                    const std::shared_ptr<Node>& second);
    // Combines normalized distance, time, and cost using the configured weights.
    double objectiveScore(double km, double minutes, double cost) const;
    // Sums the selected metric from depot through stops and back to the depot.
    double routeMetric(const std::shared_ptr<Node>& depot,
                       const std::vector<std::shared_ptr<Node>>& stops,
                       Metric metric);
    // Reads one requested metric from a leg result.
    double metricValue(const Leg& leg, Metric metric) const;
    // Groups each customer under its geographically nearest depot.
    std::unordered_map<std::shared_ptr<Node>,
                       std::vector<std::shared_ptr<Node>>>
    assignCustomersToDepots(
        const std::vector<std::shared_ptr<Node>>& customers,
        const std::vector<std::shared_ptr<Node>>& depots);
    // Constructs capacity-limited routes by repeatedly visiting the nearest customer.
    std::vector<Route> nearestMultiDepot(
        const std::vector<std::shared_ptr<Node>>& customers,
        const std::vector<std::shared_ptr<Node>>& depots);
    // Constructs routes using Clarke-Wright savings and merges routes to fit the fleet.
    std::vector<Route> savingsMultiDepot(
        const std::vector<std::shared_ptr<Node>>& customers,
        const std::vector<std::shared_ptr<Node>>& depots);
    // Joins two routes when their saved customers are at mergeable endpoints.
    std::vector<std::shared_ptr<Node>> mergeAtRouteEnds(
        const Route& first, const Route& second, const Saving& saving);
    // Searches route orientations and depots for the lowest-score legal merge.
    MergeCandidate* findBestRouteMerge(
        const std::vector<Route>& routes,
        const std::vector<std::shared_ptr<Node>>& depots);
    // Improves a route by reversing subsequences that reduce its score.
    void twoOpt(Route& route);
    // Tries a limited set of three-opt-style segment reversals.
    void threeOptLite(Route& route);
    // Finds a shortest weighted path through the road graph using Dijkstra's algorithm.
    Leg shortestLeg(const std::shared_ptr<RoadNode>& start,
                    const std::shared_ptr<RoadNode>& target);
};

#endif
