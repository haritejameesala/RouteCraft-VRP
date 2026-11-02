// File purpose: Implements constructors and small helpers for shared routing data.
// Flow:
//   Initialize customers and depots -> total route demand -> initialize road
//   vertices and directed road edges for graph search.
#include "RouteObjects.h"

#include <utility>

// Keeps the caller's location and demand data together for route planning.
Node::Node(int id, std::string name, double x, double y, int demand,
           bool isDepot)
    : id(id),
      name(std::move(name)),
      x(x),
      y(y),
      demand(demand),
      isDepot(isDepot) {}

// Totals the demand of every stop so construction can enforce vehicle capacity.
int Route::load() const {
    int totalDemand = 0;
    for (const auto& stop : stops) {
        totalDemand += stop->demand;
    }
    return totalDemand;
}

// Leaves projected coordinates unset until the map loader fills them in.
RoadNode::RoadNode(long long id, double lat, double lon)
    : id(id), lat(lat), lon(lon), x(0.0), y(0.0) {}

// Leaves an edge empty until the graph loader supplies its destination and metrics.
RoadEdge::RoadEdge() = default;

// Stores a directed connection and its measurements for all route metrics.
RoadEdge::RoadEdge(std::shared_ptr<RoadNode> to, double km, double minutes,
                   double cost)
    : to(std::move(to)), km(km), minutes(minutes), cost(cost) {}
