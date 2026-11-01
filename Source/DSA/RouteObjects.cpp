// File purpose: Implements constructors and helpers for the shared DSA data types.
// DSA part: Shared support used by Parts 1, 2, and 3.
#include "RouteObjects.h"

#include <utility>

using namespace std;

// Stores a location's identity, coordinates, demand, and depot/customer role.
Node::Node(int id, string name, double x, double y, int demand, bool isDepot)
    : id(id),
      name(move(name)),
      x(x),
      y(y),
      demand(demand),
      isDepot(isDepot) {}

// Totals the demand of every customer assigned to this route.
int Route::load() const {
    int totalDemand = 0;
    for (const auto& stop : stops) {
        totalDemand += stop->demand;
    }
    return totalDemand;
}

// Initializes geographic coordinates and leaves projected coordinates at zero.
RoadNode::RoadNode(long long id, double lat, double lon)
    : id(id), lat(lat), lon(lon), x(0.0), y(0.0) {}

// Stores a directed road connection and its three routing measurements.
RoadEdge::RoadEdge(shared_ptr<RoadNode> to, double km, double minutes,
                   double cost)
    : to(move(to)), km(km), minutes(minutes), cost(cost) {}
