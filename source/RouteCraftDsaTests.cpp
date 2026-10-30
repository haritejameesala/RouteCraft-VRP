#include "RouteOptimizer.h"

#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace {

// Fails a test group immediately with a message describing the unmet condition.
void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

// Checks round-trip route measurements and the geographic distance helper.
void testRouteMetrics() {
    unordered_map<long long, shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    auto depot = make_shared<Node>(1, "Depot", 0.0, 0.0, 0, true);
    auto customer = make_shared<Node>(1, "Customer", 3.0, 4.0, 1, false);
    const vector<shared_ptr<Node>> stops{customer};

    require(abs(optimizer.routeKm(depot, stops) - 10.0) < 1e-9,
            "Round-trip distance should be 10.");
    require(abs(optimizer.routeMinutes(depot, stops) - 20.0) < 1e-9,
            "Round-trip time should be 20.");
    require(abs(optimizer.routeCost(depot, stops) - 120.0) < 1e-9,
            "Round-trip cost should be 120.");
    require(abs(optimizer.routeDistance(depot, stops) - 10.0) < 1e-9,
            "Default weighted score should be 10.");
    require(abs(RouteOptimizer::geoKm(0.0, 0.0, 0.0, 1.0) - 111.195) <
                0.1,
            "One degree of longitude at the equator should be about 111.2 km.");
}

// Checks capacity, fleet size, and exactly-once customer coverage for one method.
void testCapacityAndCoverage(bool useSavings) {
    unordered_map<long long, shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    vector<shared_ptr<Node>> depots{
        make_shared<Node>(1, "D1", 0.0, 0.0, 0, true),
        make_shared<Node>(2, "D2", 100.0, 100.0, 0, true)};
    vector<shared_ptr<Node>> customers{
        make_shared<Node>(1, "C1", 1.0, 0.0, 2, false),
        make_shared<Node>(2, "C2", 2.0, 0.0, 3, false),
        make_shared<Node>(3, "C3", 4.0, 0.0, 1, false)};

    const auto routes = optimizer.optimize(customers, depots, 2, 5, useSavings);
    set<int> visited;
    for (const Route& route : routes) {
        require(route.load() <= 5,
                "A route must not exceed vehicle capacity.");
        for (const auto& stop : route.stops) {
            require(visited.insert(stop->id).second,
                    "A customer must not appear more than once.");
        }
    }
    require(visited.size() == customers.size(),
            "Every feasible customer must be assigned.");
    require(routes.size() <= 2,
            "A feasible scenario must fit within the requested fleet.");
}

// Verifies graph cleanup retains the largest mutually reachable node group.
void testStronglyConnectedComponent() {
    auto first = make_shared<RoadNode>(1, 0.0, 0.0);
    auto second = make_shared<RoadNode>(2, 0.0, 1.0);
    auto isolated = make_shared<RoadNode>(3, 1.0, 1.0);
    first->edges.emplace_back(second, 1.0, 1.0, 1.0);
    second->edges.emplace_back(first, 1.0, 1.0, 1.0);
    set<shared_ptr<RoadNode>> component{first, second, isolated};

    RouteOptimizer::retainLargestStronglyConnectedComponent(component);
    require(component.size() == 2,
            "The largest strongly connected component should contain two nodes.");
    require(component.count(first) == 1 && component.count(second) == 1,
            "The retained component should contain the mutually reachable "
            "nodes.");
    require(first->edges.size() == 1 && second->edges.size() == 1,
            "Edges outside the retained component should be removed.");
}

// Ensures invalid optimization input is rejected with an exception.
void testInputValidation() {
    unordered_map<long long, shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    const vector<shared_ptr<Node>> customers{
        make_shared<Node>(1, "C1", 1.0, 1.0, 1, false)};
    bool rejected = false;
    try {
        optimizer.optimize(customers, {}, 1, 5, false);
    } catch (const invalid_argument&) {
        rejected = true;
    }
    require(rejected,
            "Customers without a depot should be rejected explicitly.");
}

}  // namespace

int main() {
    try {
        // Run every independent behavior check and report a concise test result.
        testRouteMetrics();
        testCapacityAndCoverage(false);
        testCapacityAndCoverage(true);
        testStronglyConnectedComponent();
        testInputValidation();
        cout << "All RouteCraft DSA tests passed (5 test groups).\n";
        return 0;
    } catch (const exception& error) {
        cerr << "RouteCraft DSA test failed: " << error.what() << '\n';
        return 1;
    }
}
