#include "RouteObjects.h"
#include <utility>
Node::Node(int id, std::string name, double x, double y, int demand, bool isDepot)
    : id(id), name(std::move(name)), x(x), y(y), demand(demand), isDepot(isDepot) {}
std::string Node::label() const { return isDepot ? name : "C" + std::to_string(id); }
int Route::load() const { int td = 0; for (const auto& s : stops) td += s->demand; return td; }
RoadNode::RoadNode(long long id, double lat, double lon) : id(id), lat(lat), lon(lon), x(0.0), y(0.0) {}
RoadEdge::RoadEdge(std::shared_ptr<RoadNode> to, double km, double minutes, double cost)
    : to(std::move(to)), km(km), minutes(minutes), cost(cost) {}