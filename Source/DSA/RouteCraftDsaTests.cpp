// File purpose: Tests route construction, route metrics, graph connectivity, and validation.
// DSA part: Shared tests covering Parts 1, 2, and 3.
// Flow:
//   Check custom containers -> verify route metrics and both construction methods
//   -> check elastic fleet tiers and road-graph connectivity
//   -> confirm invalid requests are rejected -> report the first failure or success.
// Each test group builds a small, deterministic scenario and uses require() for assertions.
#include "RouteOptimizer.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

namespace {

struct MinFirst {
    // Makes the custom heap return the smallest integer first.
    bool operator()(int first, int second) const {
        return first > second;
    }
};

// Fails a test group immediately with a message describing the unmet condition.
void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

// Checks the custom containers used by route construction and graph search.
void testCustomContainers() {
    dsa::Array<int> values{1, 4};
    const int additions[] = {2, 3};
    values.insert(values.begin() + 1, additions, additions + 2);
    require(values.size() == 4 && values[0] == 1 && values[1] == 2 &&
                values[2] == 3 && values[3] == 4,
            "The custom dynamic array should insert a range in order.");
    values.erase(values.begin() + 1);
    require(values.size() == 3 && values[1] == 3,
            "The custom dynamic array should erase and shift elements.");

    dsa::HashMap<int, int> valuesByKey;
    for (int i = 0; i < 40; ++i) {
        valuesByKey[i] = i * 2;
    }
    for (int i = 0; i < 40; ++i) {
        require(valuesByKey.at(i) == i * 2,
                "The custom hash map should retain values through rehashing.");
    }
    require(valuesByKey.emplace(4, 99).second == false &&
                valuesByKey.at(4) == 8,
            "The custom hash map should reject duplicate keys.");

    dsa::BinaryHeap<int, MinFirst> pending;
    pending.push(7);
    pending.push(2);
    pending.push(5);
    require(pending.top() == 2,
            "The custom binary heap should return the minimum value.");
    pending.pop();
    require(pending.top() == 5,
            "The custom binary heap should restore order after removal.");
}

// Checks round-trip route measurements and the geographic distance helper.
void testRouteMetrics() {
    dsa::HashMap<long long, shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    auto depot = make_shared<Node>(1, "Depot", 0.0, 0.0, 0, true);
    auto customer = make_shared<Node>(1, "Customer", 3.0, 4.0, 1, false);
    const dsa::Array<shared_ptr<Node>> stops{customer};

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
    dsa::HashMap<long long, shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    dsa::Array<shared_ptr<Node>> depots{
        make_shared<Node>(1, "D1", 0.0, 0.0, 0, true),
        make_shared<Node>(2, "D2", 100.0, 100.0, 0, true)};
    dsa::Array<shared_ptr<Node>> customers{
        make_shared<Node>(1, "C1", 1.0, 0.0, 2, false),
        make_shared<Node>(2, "C2", 2.0, 0.0, 3, false),
        make_shared<Node>(3, "C3", 4.0, 0.0, 1, false)};

    const auto routes = optimizer.optimize(customers, depots, 2, 5, useSavings);
    dsa::UniqueSet<int> visited;
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

// Verifies elastic fleet activation order and virtual-vehicle reporting.
void testElasticFleetTiers() {
    dsa::HashMap<long long, shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    dsa::Array<shared_ptr<Node>> depots{
        make_shared<Node>(10, "D1", 0.0, 0.0, 0, true)};
    dsa::Array<shared_ptr<Node>> customers{
        make_shared<Node>(1, "C1", 1.0, 0.0, 5, false),
        make_shared<Node>(2, "C2", 2.0, 0.0, 5, false),
        make_shared<Node>(3, "C3", 3.0, 0.0, 5, false)};
    const auto routes = optimizer.optimizeElastic(customers, depots, 1, 1, 5, true);
    require(routes.size() == 3,
            "Capacity should require three routes in this test case.");
    require(routes[0].fleetTier == 0 && routes[1].fleetTier == 1 &&
                routes[2].fleetTier == 2,
            "Dedicated, ad-hoc, then virtual tiers must be assigned in order.");
    int served = 0;
    for (const auto& route : routes) served += static_cast<int>(route.stops.size());
    require(served == 3,
            "Virtual planning routes must preserve all customer assignments.");
}

// Verifies graph cleanup retains the largest mutually reachable node group.
void testStronglyConnectedComponent() {
    auto first = make_shared<RoadNode>(1, 0.0, 0.0);
    auto second = make_shared<RoadNode>(2, 0.0, 1.0);
    auto isolated = make_shared<RoadNode>(3, 1.0, 1.0);
    first->edges.emplace_back(second, 1.0, 1.0, 1.0);
    second->edges.emplace_back(first, 1.0, 1.0, 1.0);
    dsa::UniqueSet<shared_ptr<RoadNode>> component{first, second, isolated};

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
    dsa::HashMap<long long, shared_ptr<RoadNode>> roadNodes;
    RouteOptimizer optimizer(roadNodes);
    const dsa::Array<shared_ptr<Node>> customers{
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

// Runs each DSA behavior group and turns the first failure into a nonzero exit.
int main() {
    try {
        // Run every independent behavior check and report a concise test result.
        testCustomContainers();
        testRouteMetrics();
        testCapacityAndCoverage(false);
        testCapacityAndCoverage(true);
        testElasticFleetTiers();
        testStronglyConnectedComponent();
        testInputValidation();
        cout << "All RouteCraft DSA tests passed (7 test groups).\n";
        return 0;
    } catch (const exception& error) {
        cerr << "RouteCraft DSA test failed: " << error.what() << '\n';
        return 1;
    }
}
