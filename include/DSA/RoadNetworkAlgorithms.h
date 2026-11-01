// File purpose: Declares road-graph search, connectivity, and geographic algorithms.
// DSA part: Part 3 - Road-network algorithms.
#ifndef ROAD_NETWORK_ALGORITHMS_H
#define ROAD_NETWORK_ALGORITHMS_H

#include "RouteObjects.h"

#include <functional>
#include <memory>
#include <set>
#include <unordered_map>

class RoadNetworkAlgorithms {
public:
    static std::shared_ptr<RoadNode> nearestNode(
        const std::unordered_map<long long, std::shared_ptr<RoadNode>>& nodes,
        double x, double y);
    static Leg shortestLeg(const std::shared_ptr<RoadNode>& start,
                           const std::shared_ptr<RoadNode>& target,
                           const std::function<double(double, double, double)>&
                               objectiveScore);
    static void retainLargestStronglyConnectedComponent(
        std::set<std::shared_ptr<RoadNode>>& used);
    static double geoKm(double lat1, double lon1, double lat2, double lon2);
};

#endif
