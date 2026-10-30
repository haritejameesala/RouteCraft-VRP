#ifndef ROUTE_OBJECTS_H
#define ROUTE_OBJECTS_H

#include <memory>
#include <string>
#include <vector>

// Forward declaration lets RoadNode store outgoing edges by value below.
struct RoadEdge;

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
    // Returns a depot's name or a generated customer label.
    std::string label() const;
};

// One vehicle's trip: its starting depot followed by its customer stops.
struct Route {
    std::shared_ptr<Node> depot;              // Depot where this route begins/ends.
    std::vector<std::shared_ptr<Node>> stops;  // Customers in visit order.

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
    std::vector<RoadEdge> edges;   // Directed connections leaving this vertex.

    // Creates a graph vertex; projected coordinates are initialized separately.
    RoadNode(long long id, double lat, double lon);
};

// Directed road connection and its cost in each supported metric.
struct RoadEdge {
    std::weak_ptr<RoadNode> to;  // Destination; weak ownership avoids graph cycles.
    double km;                   // Distance along this road segment.
    double minutes;              // Travel time along this road segment.
    double cost;                 // Monetary cost of this road segment.

    // Creates an edge to a road node with its distance, time, and cost.
    RoadEdge(std::shared_ptr<RoadNode> to, double km, double minutes,
             double cost);
};

// Result of routing between two locations, including its graph path and metrics.
struct Leg {
    std::vector<std::shared_ptr<RoadNode>> path;  // Road vertices in travel order.
    double km = 0.0;                             // Total distance.
    double minutes = 0.0;                        // Total travel time.
    double cost = 0.0;                           // Total travel cost.
    double score = 0.0;                          // Weighted optimization score.
};

#endif
