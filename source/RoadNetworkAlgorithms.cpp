// File purpose: Implements nearest-node lookup, Dijkstra, SCC filtering, and geo distance.
// DSA part: Part 3 - Road-network algorithms.
#include "RoadNetworkAlgorithms.h"
#include "RouteOptimizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stack>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

namespace {

struct TraversalFrame {
    shared_ptr<RoadNode> node;
    size_t nextEdge = 0;
};

}  // namespace

Leg RouteOptimizer::leg(
    const shared_ptr<Node>& from, const shared_ptr<Node>& to) {
    const string key =
        to_string(from->id) + ":" + (from->isDepot ? "D" : "C") + ":" +
        to_string(to->id) + ":" + (to->isDepot ? "D" : "C");
    const auto cached = legCache.find(key);
    if (cached != legCache.end()) {
        return cached->second;
    }

    Leg result;
    if (roadNetworkLoaded && !roadNodes.empty()) {
        const auto start = nearestRoadNode(from->x, from->y);
        const auto end = nearestRoadNode(to->x, to->y);
        if (start && end) {
            result = RoadNetworkAlgorithms::shortestLeg(
                start, end,
                [this](double km, double minutes, double cost) {
                    return objectiveScore(km, minutes, cost);
                });
            if (!result.path.empty()) {
                legCache[key] = result;
                return result;
            }
        }
    }

    const double straightLine = hypot(from->x - to->x, from->y - to->y);
    result.km = straightLine;
    result.minutes = straightLine * 2.0;
    result.cost = straightLine * 12.0;
    result.score = objectiveScore(result.km, result.minutes, result.cost);
    legCache[key] = result;
    return result;
}

shared_ptr<RoadNode> RouteOptimizer::nearestRoadNode(double x, double y) {
    return RoadNetworkAlgorithms::nearestNode(roadNodes, x, y);
}

shared_ptr<RoadNode> RoadNetworkAlgorithms::nearestNode(
    const unordered_map<long long, shared_ptr<RoadNode>>& nodes, double x,
    double y) {
    shared_ptr<RoadNode> nearest = nullptr;
    double best = numeric_limits<double>::max();
    for (const auto& entry : nodes) {
        const auto& node = entry.second;
        const double currentDistance = hypot(node->x - x, node->y - y);
        if (currentDistance < best) {
            best = currentDistance;
            nearest = node;
        }
    }
    return nearest;
}

Leg RoadNetworkAlgorithms::shortestLeg(
    const shared_ptr<RoadNode>& start, const shared_ptr<RoadNode>& target,
    const function<double(double, double, double)>& objectiveScore) {
    Leg result;
    if (start == target) {
        result.path.push_back(start);
        return result;
    }

    unordered_map<shared_ptr<RoadNode>, double> distances;
    unordered_map<shared_ptr<RoadNode>, shared_ptr<RoadNode>> previous;
    auto compare = [&](const shared_ptr<RoadNode>& first,
                       const shared_ptr<RoadNode>& second) {
        return distances[first] > distances[second];
    };
    priority_queue<shared_ptr<RoadNode>, vector<shared_ptr<RoadNode>>,
                   decltype(compare)>
        pending(compare);
    set<shared_ptr<RoadNode>> settled;
    distances[start] = 0.0;
    pending.push(start);

    while (!pending.empty()) {
        const auto current = pending.top();
        pending.pop();
        if (!settled.insert(current).second) {
            continue;
        }
        if (current == target) {
            break;
        }

        for (const auto& edge : current->edges) {
            const auto next = edge.to.lock();
            if (!next) {
                continue;
            }
            const double candidateDistance =
                distances[current] +
                objectiveScore(edge.km, edge.minutes, edge.cost);
            if (distances.find(next) == distances.end() ||
                candidateDistance < distances[next]) {
                distances[next] = candidateDistance;
                previous[next] = current;
                pending.push(next);
            }
        }
    }

    if (distances.find(target) == distances.end()) {
        result.km = geoKm(start->lat, start->lon, target->lat, target->lon);
        result.minutes = result.km * 2.0;
        result.cost = result.km * 12.0;
        result.score = objectiveScore(result.km, result.minutes, result.cost);
        result.path = {start, target};
        return result;
    }

    auto current = target;
    while (current) {
        result.path.push_back(current);
        if (current == start) {
            break;
        }
        current = previous[current];
    }
    reverse(result.path.begin(), result.path.end());

    for (size_t i = 0; i < result.path.size() - 1; ++i) {
        for (const auto& edge : result.path[i]->edges) {
            if (edge.to.lock() == result.path[i + 1]) {
                result.km += edge.km;
                result.minutes += edge.minutes;
                result.cost += edge.cost;
                break;
            }
        }
    }
    result.score = objectiveScore(result.km, result.minutes, result.cost);
    return result;
}

void RoadNetworkAlgorithms::retainLargestStronglyConnectedComponent(
    set<shared_ptr<RoadNode>>& used) {
    vector<shared_ptr<RoadNode>> nodes(used.begin(), used.end());
    unordered_map<shared_ptr<RoadNode>, vector<shared_ptr<RoadNode>>>
        reverseEdges;
    for (const auto& node : nodes) {
        for (const auto& edge : node->edges) {
            if (auto target = edge.to.lock()) {
                reverseEdges[target].push_back(node);
            }
        }
    }

    set<shared_ptr<RoadNode>> visited;
    stack<shared_ptr<RoadNode>> finishOrder;
    for (const auto& root : nodes) {
        if (!visited.insert(root).second) {
            continue;
        }
        stack<TraversalFrame> traversal;
        traversal.push({root, 0});
        while (!traversal.empty()) {
            auto& frame = traversal.top();
            if (frame.nextEdge < frame.node->edges.size()) {
                auto next = frame.node->edges[frame.nextEdge++].to.lock();
                if (next && used.count(next) && visited.insert(next).second) {
                    traversal.push({next, 0});
                }
            } else {
                finishOrder.push(frame.node);
                traversal.pop();
            }
        }
    }

    set<shared_ptr<RoadNode>> assigned;
    set<shared_ptr<RoadNode>> largestComponent;
    while (!finishOrder.empty()) {
        const auto root = finishOrder.top();
        finishOrder.pop();
        if (!assigned.insert(root).second) {
            continue;
        }

        set<shared_ptr<RoadNode>> component;
        stack<shared_ptr<RoadNode>> traversal;
        traversal.push(root);
        while (!traversal.empty()) {
            const auto node = traversal.top();
            traversal.pop();
            component.insert(node);
            for (const auto& predecessor : reverseEdges[node]) {
                if (assigned.insert(predecessor).second) {
                    traversal.push(predecessor);
                }
            }
        }
        if (component.size() > largestComponent.size()) {
            largestComponent = component;
        }
    }

    for (auto it = used.begin(); it != used.end();) {
        if (!largestComponent.count(*it)) {
            it = used.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& node : used) {
        node->edges.erase(
            remove_if(node->edges.begin(), node->edges.end(),
                      [&](const RoadEdge& edge) {
                          const auto target = edge.to.lock();
                          return !target || !used.count(target);
                      }),
            node->edges.end());
    }
}

double RoadNetworkAlgorithms::geoKm(
    double lat1, double lon1, double lat2, double lon2) {
    constexpr double pi = 3.14159265358979323846;
    constexpr double earthRadiusKm = 6371.0;
    const double deltaLatitude = (lat2 - lat1) * pi / 180.0;
    const double deltaLongitude = (lon2 - lon1) * pi / 180.0;
    const double a =
        sin(deltaLatitude / 2) * sin(deltaLatitude / 2) +
        cos(lat1 * pi / 180.0) * cos(lat2 * pi / 180.0) *
            sin(deltaLongitude / 2) * sin(deltaLongitude / 2);
    return 2 * earthRadiusKm * asin(sqrt(a));
}
