#ifndef ROUTE_OBJECTS_H
#define ROUTE_OBJECTS_H
#include <string>
#include <vector>
#include <memory>
struct RoadEdge;
struct Node {
    int id; std::string name; double x; double y; int demand; bool isDepot;
    Node(int id, std::string name, double x, double y, int demand, bool isDepot);
    std::string label() const;
};
struct Route {
    std::shared_ptr<Node> depot; std::vector<std::shared_ptr<Node>> stops;
    int load() const;
};
struct RoadNode {
    long long id; double lat; double lon; double x; double y; std::vector<RoadEdge> edges;
    RoadNode(long long id, double lat, double lon);
};
struct RoadEdge {
    std::weak_ptr<RoadNode> to; double km; double minutes; double cost;
    RoadEdge(std::shared_ptr<RoadNode> to, double km, double minutes, double cost);
};
struct Leg {
    std::vector<std::shared_ptr<RoadNode>> path;
    double km = 0.0; double minutes = 0.0; double cost = 0.0; double score = 0.0;
};
#endif