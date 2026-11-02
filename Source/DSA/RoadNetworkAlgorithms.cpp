// File purpose: Provides the road-graph operations used by route optimization.
// Flow:
//   Route leg -> check cache -> snap endpoints -> run weighted Dijkstra
//             -> return road metrics, or use a geographic fallback if disconnected.
//   Graph cleanup -> build reverse edges -> run Kosaraju passes -> keep the largest SCC.
//   Supporting helpers -> find the nearest graph node and calculate haversine distance.
// RouteOptimizer wrappers keep the existing public API connected to these algorithms.
#include "RoadNetworkAlgorithms.h"

#include "RouteOptimizer.h"

#include <cmath>
#include <limits>

namespace {

// Keeps a road vertex beside its current Dijkstra score in the heap.
struct HeapEntry {
    std::shared_ptr<RoadNode> node;
    double score;
};

// Makes the heap return the entry with the smallest path score first.
struct MinScoreFirst {
    bool operator()(const HeapEntry& first, const HeapEntry& second) const {
        return first.score > second.score;
    }
};

// Remembers where iterative depth-first search should continue from a vertex.
struct TraversalFrame {
    std::shared_ptr<RoadNode> node;
    std::size_t nextEdge;
};

}  // namespace

// Uses cached road routing when possible and falls back to map-coordinate distance.
Leg RouteOptimizer::leg(const std::shared_ptr<Node>& from,
                        const std::shared_ptr<Node>& to) {
    const std::string key =
        std::to_string(from->id) + ":" + (from->isDepot ? "D" : "C") + ":" +
        std::to_string(to->id) + ":" + (to->isDepot ? "D" : "C");
    const auto cached = legCache.find(key);
    if (cached != legCache.end()) {
        return cached->second;
    }

    Leg result;
    if (roadNetworkLoaded && !roadNodes.empty()) {
        // Snap each endpoint to the closest graph vertex before searching roads.
        const auto start = nearestRoadNode(from->x, from->y);
        const auto end = nearestRoadNode(to->x, to->y);
        if (start && end) {
            result = RoadNetworkAlgorithms::shortestLeg(
                start, end, weightDistance, weightTime, weightCost);
            if (!result.path.empty()) {
                legCache[key] = result;
                return result;
            }
        }
    }

    const double straightLine = std::hypot(from->x - to->x, from->y - to->y);
    result.km = straightLine;
    result.minutes = straightLine * 2.0;
    result.cost = straightLine * 12.0;
    result.score = objectiveScore(result.km, result.minutes, result.cost);
    legCache[key] = result;
    return result;
}

// Lets callers find the same nearest graph vertex used by road routing.
std::shared_ptr<RoadNode> RouteOptimizer::nearestRoadNode(double x, double y) {
    return RoadNetworkAlgorithms::nearestNode(roadNodes, x, y);
}

// Checks every graph vertex and returns the one with the smallest map distance.
std::shared_ptr<RoadNode> RoadNetworkAlgorithms::nearestNode(
    const dsa::HashMap<long long, std::shared_ptr<RoadNode>>& nodes, double x,
    double y) {
    std::shared_ptr<RoadNode> nearest;
    double best = std::numeric_limits<double>::max();
    for (const auto& entry : nodes) {
        const auto& node = entry.second;
        const double currentDistance = std::hypot(node->x - x, node->y - y);
        if (currentDistance < best) {
            best = currentDistance;
            nearest = node;
        }
    }
    return nearest;
}

// Runs Dijkstra with the configured score and rebuilds the chosen path afterward.
Leg RoadNetworkAlgorithms::shortestLeg(
    const std::shared_ptr<RoadNode>& start,
    const std::shared_ptr<RoadNode>& target, double weightDistance,
    double weightTime, double weightCost) {
    const auto objectiveScore =
        [weightDistance, weightTime, weightCost](double km, double minutes,
                                                 double cost) {
            return weightDistance * km + weightTime * (minutes / 60.0 * 30.0) +
                   weightCost * (cost / 12.0);
        };
    Leg result;
    if (start == target) {
        result.path.push_back(start);
        return result;
    }

    dsa::HashMap<std::shared_ptr<RoadNode>, double> distances;
    dsa::HashMap<std::shared_ptr<RoadNode>, std::shared_ptr<RoadNode>> previous;
    dsa::BinaryHeap<HeapEntry, MinScoreFirst> pending;
    dsa::UniqueSet<std::shared_ptr<RoadNode>> settled;
    distances[start] = 0.0;
    pending.push(HeapEntry{start, 0.0});

    while (!pending.empty()) {
        const HeapEntry entry = pending.top();
        pending.pop();
        const auto current = entry.node;
        if (settled.contains(current)) {
            continue;
        }
        settled.insert(current);
        if (current == target) {
            break;
        }

        for (const auto& edge : current->edges) {
            const auto next = edge.to.lock();
            if (!next) {
                continue;
            }
            const double candidateDistance =
                distances.at(current) +
                objectiveScore(edge.km, edge.minutes, edge.cost);
            const auto knownDistance = distances.find(next);
            if (knownDistance == distances.end() ||
                candidateDistance < knownDistance->second) {
                // A cheaper route replaces the old predecessor and gets a new heap entry.
                distances[next] = candidateDistance;
                previous[next] = current;
                pending.push(HeapEntry{next, candidateDistance});
            }
        }
    }

    if (distances.find(target) == distances.end()) {
        // Keep the route usable even when the road graph has no connecting path.
        result.km = geoKm(start->lat, start->lon, target->lat, target->lon);
        result.minutes = result.km * 2.0;
        result.cost = result.km * 12.0;
        result.score = objectiveScore(result.km, result.minutes, result.cost);
        result.path.push_back(start);
        result.path.push_back(target);
        return result;
    }

    // Walk backward from the target, then reverse the collected vertices.
    auto current = target;
    while (current) {
        result.path.push_back(current);
        if (current == start) {
            break;
        }
        const auto prior = previous.find(current);
        if (prior == previous.end()) {
            result.path.clear();
            return result;
        }
        current = prior->second;
    }
    for (std::size_t first = 0, last = result.path.size();
         first < last && first < --last; ++first) {
        auto value = std::move(result.path[first]);
        result.path[first] = std::move(result.path[last]);
        result.path[last] = std::move(value);
    }

    // Sum the real edge measurements along the selected vertex sequence.
    for (std::size_t i = 0; i + 1 < result.path.size(); ++i) {
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

// Uses Kosaraju's two DFS passes to keep the graph's largest strongly connected set.
void RoadNetworkAlgorithms::retainLargestStronglyConnectedComponent(
    dsa::UniqueSet<std::shared_ptr<RoadNode>>& used) {
    dsa::Array<std::shared_ptr<RoadNode>> nodes;
    nodes.reserve(used.size());
    for (const auto& node : used) {
        nodes.push_back(node);
    }

    // Reverse adjacency is needed for the second pass over incoming edges.
    dsa::HashMap<std::shared_ptr<RoadNode>,
                 dsa::Array<std::shared_ptr<RoadNode>>>
        reverseEdges;
    for (const auto& node : nodes) {
        for (const auto& edge : node->edges) {
            const auto target = edge.to.lock();
            if (target) {
                reverseEdges[target].push_back(node);
            }
        }
    }

    // First pass records finishing order without using recursive calls.
    dsa::UniqueSet<std::shared_ptr<RoadNode>> visited;
    dsa::Stack<std::shared_ptr<RoadNode>> finishOrder;
    for (const auto& root : nodes) {
        if (!visited.insert(root).second) {
            continue;
        }
        dsa::Stack<TraversalFrame> traversal;
        traversal.push(TraversalFrame{root, 0});
        while (!traversal.empty()) {
            TraversalFrame& frame = traversal.top();
            if (frame.nextEdge < frame.node->edges.size()) {
                const auto next =
                    frame.node->edges[frame.nextEdge++].to.lock();
                if (next && used.contains(next) &&
                    visited.insert(next).second) {
                    traversal.push(TraversalFrame{next, 0});
                }
            } else {
                finishOrder.push(frame.node);
                traversal.pop();
            }
        }
    }

    // Second pass groups vertices from latest finish time through reverse edges.
    dsa::UniqueSet<std::shared_ptr<RoadNode>> assigned;
    dsa::UniqueSet<std::shared_ptr<RoadNode>> largestComponent;
    while (!finishOrder.empty()) {
        const auto root = finishOrder.top();
        finishOrder.pop();
        if (!assigned.insert(root).second) {
            continue;
        }

        dsa::UniqueSet<std::shared_ptr<RoadNode>> component;
        dsa::Stack<std::shared_ptr<RoadNode>> traversal;
        traversal.push(root);
        while (!traversal.empty()) {
            const auto node = traversal.top();
            traversal.pop();
            component.insert(node);
            const auto predecessors = reverseEdges.find(node);
            if (predecessors == reverseEdges.end()) {
                continue;
            }
            for (const auto& predecessor : predecessors->second) {
                if (assigned.insert(predecessor).second) {
                    traversal.push(predecessor);
                }
            }
        }
        if (component.size() > largestComponent.size()) {
            largestComponent = component;
        }
    }

    // Drop vertices outside the winner and remove any edges that point outside it.
    for (auto node = used.begin(); node != used.end();) {
        if (!largestComponent.contains(*node)) {
            node = used.erase(node);
        } else {
            ++node;
        }
    }
    for (const auto& node : used) {
        for (std::size_t i = 0; i < node->edges.size();) {
            const auto target = node->edges[i].to.lock();
            if (!target || !used.contains(target)) {
                node->edges.erase(node->edges.begin() + i);
            } else {
                ++i;
            }
        }
    }
}

// Uses the haversine formula to measure distance along the Earth's surface.
double RoadNetworkAlgorithms::geoKm(double lat1, double lon1, double lat2,
                                    double lon2) {
    constexpr double pi = 3.14159265358979323846;
    constexpr double earthRadiusKm = 6371.0;
    const double deltaLatitude = (lat2 - lat1) * pi / 180.0;
    const double deltaLongitude = (lon2 - lon1) * pi / 180.0;
    const double a =
        std::sin(deltaLatitude / 2) * std::sin(deltaLatitude / 2) +
        std::cos(lat1 * pi / 180.0) * std::cos(lat2 * pi / 180.0) *
            std::sin(deltaLongitude / 2) * std::sin(deltaLongitude / 2);
    return 2 * earthRadiusKm * std::asin(std::sqrt(a));
}

// Keeps the older optimizer entry point while sharing the graph implementation.
void RouteOptimizer::retainLargestStronglyConnectedComponent(
    dsa::UniqueSet<std::shared_ptr<RoadNode>>& used) {
    RoadNetworkAlgorithms::retainLargestStronglyConnectedComponent(used);
}

// Keeps geographic-distance callers on the same road-algorithm implementation.
double RouteOptimizer::geoKm(double lat1, double lon1, double lat2,
                             double lon2) {
    return RoadNetworkAlgorithms::geoKm(lat1, lon1, lat2, lon2);
}
