// File purpose: Declares road-graph search, connectivity, and geo algorithms.
// Flow:
//   Find nearby graph vertices -> calculate weighted road paths -> retain the
//   largest strongly connected component -> provide geographic distance.
#ifndef ROAD_NETWORK_ALGORITHMS_H
#define ROAD_NETWORK_ALGORITHMS_H

#include "RouteObjects.h"

#include <memory>

class RoadNetworkAlgorithms {
public:
    // Finds the road vertex nearest to the projected coordinates.
    static std::shared_ptr<RoadNode> nearestNode(
        const dsa::HashMap<long long, std::shared_ptr<RoadNode>>& nodes,
        double x, double y);
    // Finds the minimum-objective road path and totals its measurements.
    static Leg shortestLeg(
        const std::shared_ptr<RoadNode>& start,
        const std::shared_ptr<RoadNode>& target, double weightDistance,
        double weightTime, double weightCost);
    // Retains the largest SCC and removes edges leading outside it.
    static void retainLargestStronglyConnectedComponent(
        dsa::UniqueSet<std::shared_ptr<RoadNode>>& used);
    // Calculates great-circle distance in kilometers.
    static double geoKm(double lat1, double lon1, double lat2, double lon2);
};

#endif
