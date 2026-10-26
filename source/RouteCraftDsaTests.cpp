#include "RouteOptimizer.h"

#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testRouteMetrics() {
    std::unordered_map<long long, std::shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    auto depot = std::make_shared<Node>(1, "Depot", 0.0, 0.0, 0, true);
    auto customer = std::make_shared<Node>(1, "Customer", 3.0, 4.0, 1, false);
    const std::vector<std::shared_ptr<Node>> stops{customer};

    require(std::abs(optimizer.routeKm(depot, stops) - 10.0) < 1e-9, "Round-trip distance should be 10.");
    require(std::abs(optimizer.routeMinutes(depot, stops) - 20.0) < 1e-9, "Round-trip time should be 20.");
    require(std::abs(optimizer.routeCost(depot, stops) - 120.0) < 1e-9, "Round-trip cost should be 120.");
    require(std::abs(optimizer.routeDistance(depot, stops) - 10.0) < 1e-9, "Default weighted score should be 10.");
    require(std::abs(RouteOptimizer::geoKm(0.0, 0.0, 0.0, 1.0) - 111.195) < 0.1,
            "One degree of longitude at the equator should be about 111.2 km.");
}

void testCapacityAndCoverage(bool useSavings) {
    std::unordered_map<long long, std::shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    std::vector<std::shared_ptr<Node>> depots{
        std::make_shared<Node>(1, "D1", 0.0, 0.0, 0, true),
        std::make_shared<Node>(2, "D2", 100.0, 100.0, 0, true)};
    std::vector<std::shared_ptr<Node>> customers{
        std::make_shared<Node>(1, "C1", 1.0, 0.0, 2, false),
        std::make_shared<Node>(2, "C2", 2.0, 0.0, 3, false),
        std::make_shared<Node>(3, "C3", 4.0, 0.0, 1, false)};

    const auto routes = optimizer.optimize(customers, depots, 2, 5, useSavings);
    std::set<int> visited;
    for (const Route& route : routes) {
        require(route.load() <= 5, "A route must not exceed vehicle capacity.");
        for (const auto& stop : route.stops) {
            require(visited.insert(stop->id).second, "A customer must not appear more than once.");
        }
    }
    require(visited.size() == customers.size(), "Every feasible customer must be assigned.");
    require(routes.size() <= 2, "A feasible scenario must fit within the requested fleet.");
}

void testStronglyConnectedComponent() {
    auto first = std::make_shared<RoadNode>(1, 0.0, 0.0);
    auto second = std::make_shared<RoadNode>(2, 0.0, 1.0);
    auto isolated = std::make_shared<RoadNode>(3, 1.0, 1.0);
    first->edges.emplace_back(second, 1.0, 1.0, 1.0);
    second->edges.emplace_back(first, 1.0, 1.0, 1.0);
    std::set<std::shared_ptr<RoadNode>> component{first, second, isolated};

    RouteOptimizer::retainLargestStronglyConnectedComponent(component);
    require(component.size() == 2, "The largest strongly connected component should contain two nodes.");
    require(component.count(first) == 1 && component.count(second) == 1,
            "The retained component should contain the mutually reachable nodes.");
    require(first->edges.size() == 1 && second->edges.size() == 1,
            "Edges outside the retained component should be removed.");
}

void testInputValidation() {
    std::unordered_map<long long, std::shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    const std::vector<std::shared_ptr<Node>> customers{
        std::make_shared<Node>(1, "C1", 1.0, 1.0, 1, false)};
    bool rejected = false;
    try {
        optimizer.optimize(customers, {}, 1, 5, false);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Customers without a depot should be rejected explicitly.");
}

}  // namespace

int main() {
    try {
        testRouteMetrics();
        testCapacityAndCoverage(false);
        testCapacityAndCoverage(true);
        testStronglyConnectedComponent();
        testInputValidation();
        std::cout << "All RouteCraft DSA tests passed (5 test groups).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "RouteCraft DSA test failed: " << error.what() << '\n';
        return 1;
    }
}
