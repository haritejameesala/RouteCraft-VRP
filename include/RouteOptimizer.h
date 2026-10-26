#ifndef ROUTE_OPTIMIZER_H
#define ROUTE_OPTIMIZER_H
#include "RouteObjects.h"
#include <unordered_map>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>
class RouteOptimizer {
public:
    explicit RouteOptimizer(std::unordered_map<long long, std::shared_ptr<RoadNode>>& roadNodes);
    void setWeights(double distance, double time, double cost);
    void clearCache();
    void setRoadNetworkLoaded(bool loaded);
    std::vector<Route> optimize(const std::vector<std::shared_ptr<Node>>& customers, const std::vector<std::shared_ptr<Node>>& depots, int vehicleCount, int capacity, bool useSavings);
    double routeDistance(const std::shared_ptr<Node>& depot, const std::vector<std::shared_ptr<Node>>& stops);
    double routeKm(const std::shared_ptr<Node>& depot, const std::vector<std::shared_ptr<Node>>& stops);
    double routeMinutes(const std::shared_ptr<Node>& depot, const std::vector<std::shared_ptr<Node>>& stops);
    double routeCost(const std::shared_ptr<Node>& depot, const std::vector<std::shared_ptr<Node>>& stops);
    Leg leg(const std::shared_ptr<Node>& from, const std::shared_ptr<Node>& to);
    std::shared_ptr<RoadNode> nearestRoadNode(double x, double y);
    static void retainLargestStronglyConnectedComponent(std::set<std::shared_ptr<RoadNode>>& used);
    static double geoKm(double lat1, double lon1, double lat2, double lon2);
private:
    enum class Metric { DISTANCE, TIME, COST, SCORE };
    struct Saving {
        std::shared_ptr<Node> first; std::shared_ptr<Node> second; double value;
        Saving(std::shared_ptr<Node> f, std::shared_ptr<Node> s, double v) : first(f), second(s), value(v) {}
    };
    struct MergeCandidate {
        int firstIndex; int secondIndex; std::shared_ptr<Node> depot; std::vector<std::shared_ptr<Node>> stops;
        MergeCandidate(int fIdx, int sIdx, std::shared_ptr<Node> d, std::vector<std::shared_ptr<Node>> s) : firstIndex(fIdx), secondIndex(sIdx), depot(d), stops(s) {}
    };
    struct TraversalFrame {
        std::shared_ptr<RoadNode> node; size_t nextEdge;
        explicit TraversalFrame(std::shared_ptr<RoadNode> n) : node(n), nextEdge(0) {}
    };
    std::unordered_map<long long, std::shared_ptr<RoadNode>>& roadNodes;
    std::unordered_map<std::string, Leg> legCache;
    double weightDistance = 0.40; double weightTime = 0.35; double weightCost = 0.25;
    bool roadNetworkLoaded = false; int vehicleCount = 0; int capacity = 0;
    double distance(const std::shared_ptr<Node>& first, const std::shared_ptr<Node>& second);
    double objectiveScore(double km, double minutes, double cost) const;
    double routeMetric(const std::shared_ptr<Node>& depot, const std::vector<std::shared_ptr<Node>>& stops, Metric metric);
    double metricValue(const Leg& l, Metric metric) const;
    std::unordered_map<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>> assignCustomersToDepots(const std::vector<std::shared_ptr<Node>>& customers, const std::vector<std::shared_ptr<Node>>& depots);
    std::vector<Route> nearestMultiDepot(const std::vector<std::shared_ptr<Node>>& customers, const std::vector<std::shared_ptr<Node>>& depots);
    std::vector<Route> savingsMultiDepot(const std::vector<std::shared_ptr<Node>>& customers, const std::vector<std::shared_ptr<Node>>& depots);
    std::vector<std::shared_ptr<Node>> mergeAtRouteEnds(const Route& first, const Route& second, const Saving& saving);
    MergeCandidate* findBestRouteMerge(const std::vector<Route>& routes, const std::vector<std::shared_ptr<Node>>& depots);
    void twoOpt(Route& route); void threeOptLite(Route& route);
    Leg shortestLeg(const std::shared_ptr<RoadNode>& start, const std::shared_ptr<RoadNode>& target);
};
#endif