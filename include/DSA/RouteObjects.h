// File purpose: Declares the shared route, location, and road-graph data types.
// DSA part: Shared support used by Parts 1, 2, and 3.
// Flow:
//   Node represents a depot or customer -> Route groups ordered customer stops
//   under a depot -> RoadNode and RoadEdge represent the optional street graph
//   -> Leg carries the selected path and its distance, time, cost, and score.
#ifndef ROUTE_OBJECTS_H
#define ROUTE_OBJECTS_H

#include "DsaContainers.h"

#include <memory>
#include <string>

struct RoadNode;

// Directed road connection and its cost in each supported metric.
struct RoadEdge {
    std::weak_ptr<RoadNode> to;
    double km = 0.0;
    double minutes = 0.0;
    double cost = 0.0;

    RoadEdge();
    RoadEdge(std::shared_ptr<RoadNode> to, double km, double minutes,
             double cost);
};

// A customer or depot used as a stop in a delivery route.
struct Node {
    int id;                 // Identifier used by the application and JNI bridge.
    std::string name;       // Display name; customer labels are generated from id.
    double x;               // Projected map coordinate used for routing.
    double y;               // Projected map coordinate used for routing.
    int demand;             // Capacity consumed when this customer is visited.
    bool isDepot;           // Distinguishes depots from customers.

    // Initializes a location and its routing attributes.
    Node(int id, std::string name, double x, double y, int demand, bool isDepot);
};

// One vehicle's trip: its starting depot followed by its customer stops.
struct Route {
    std::shared_ptr<Node> depot;              // Depot where this route begins/ends.
    dsa::Array<std::shared_ptr<Node>> stops;
    int fleetTier = 0;  // 0=dedicated, 1=ad-hoc, 2=virtual planning route.

    // Adds the demands of all stops to get the vehicle load.
    int load() const;
};

// A vertex in the road graph, with map coordinates and outgoing connections.
struct RoadNode {
    long long id;                  // Road-network identifier.
    double lat;                    // Geographic latitude.
    double lon;                    // Geographic longitude.
    double x;                      // Projected x coordinate used for matching.
    double y;                      // Projected y coordinate used for matching.
    dsa::Array<RoadEdge> edges;

    // Creates a graph vertex; projected coordinates are initialized separately.
    RoadNode(long long id, double lat, double lon);
};

// Result of routing between two locations, including its graph path and metrics.
struct Leg {
    dsa::Array<std::shared_ptr<RoadNode>> path;
    double km = 0.0;                             // Total distance.
    double minutes = 0.0;                        // Total travel time.
    double cost = 0.0;                           // Total travel cost.
    double score = 0.0;                          // Weighted optimization score.
};

#endif
