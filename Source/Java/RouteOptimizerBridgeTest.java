import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class RouteOptimizerBridgeTest {
    private RouteOptimizerBridgeTest() {}

    private static void require(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private static void requireNear(double actual, double expected, String message) {
        require(Math.abs(actual - expected) < 1e-9, message + ": expected " + expected + ", got " + actual);
    }

    public static void main(String[] args) {
        require("km (OSM road distance)".equals(RouteCraftVRP.distanceUnitLabel(true)),
                "A loaded OSM graph should label road distance in kilometres.");
        require("map units (no scale)".equals(RouteCraftVRP.distanceUnitLabel(false)),
                "Coordinate-only routes should not be labelled as kilometres.");

        Map<Long, RoadNode> roadNodes = new LinkedHashMap<>();
        RouteOptimizerBridge bridge = new RouteOptimizerBridge(roadNodes);
        try {
            Node depot = new Node(10, "D1", 0.0, 0.0, 0, true);
            Node customer = new Node(1, "C1", 3.0, 0.0, 1, false);
            requireNear(bridge.routeKm(depot, List.of(customer)), 6.0, "Fallback round-trip distance");

            RoadNode first = new RoadNode(100L, 0.0, 0.0);
            RoadNode second = new RoadNode(101L, 0.0, 1.0);
            RoadNode isolated = new RoadNode(102L, 1.0, 1.0);
            first.x = 0.0;
            first.y = 0.0;
            second.x = 3.0;
            second.y = 0.0;
            isolated.x = 50.0;
            isolated.y = 50.0;
            first.edges.add(new RoadEdge(second, 2.0, 1.0, 3.0));
            second.edges.add(new RoadEdge(first, 2.0, 1.0, 3.0));
            roadNodes.put(first.id, first);
            roadNodes.put(second.id, second);
            roadNodes.put(isolated.id, isolated);

            bridge.setRoadGraph(roadNodes);
            require(roadNodes.size() == 2, "The graph adapter should retain only the largest SCC.");
            require(bridge.nearestRoadNode(0.0, 0.0) == first, "Nearest road-node lookup should return the matching node.");
            bridge.setRoadNetworkLoaded(true);

            Leg leg = bridge.leg(depot, customer);
            requireNear(leg.km, 2.0, "Road-leg distance");
            requireNear(leg.minutes, 1.0, "Road-leg time");
            requireNear(leg.cost, 3.0, "Road-leg cost");
            require(leg.path.size() == 2, "Road-leg geometry should include both road nodes.");
            requireNear(bridge.routeKm(depot, List.of(customer)), 4.0, "Road-network round-trip distance");
            requireNear(bridge.routeMinutes(depot, List.of(customer)), 2.0, "Road-network round-trip time");
            requireNear(bridge.routeCost(depot, List.of(customer)), 6.0, "Road-network round-trip cost");
            require(Math.abs(bridge.geoKm(0.0, 0.0, 0.0, 1.0) - 111.195) < 0.1,
                    "Geographic distance should be about 111.2 km per equatorial longitude degree.");

            List<Route> routes = bridge.optimize(List.of(customer), List.of(depot), 1, 1, true);
            require(routes.size() == 1, "The JNI optimizer should return one feasible route.");
            require(routes.get(0).depot == depot, "The route should retain the Java depot instance.");
            require(routes.get(0).stops.size() == 1 && routes.get(0).stops.get(0) == customer,
                    "The route should retain the Java customer instance.");

            List<Node> elasticCustomers = List.of(
                new Node(21, "C21", 1.0, 1.0, 1, false),
                new Node(22, "C22", 2.0, 2.0, 1, false),
                new Node(23, "C23", 3.0, 3.0, 1, false));
            List<Route> elasticRoutes = bridge.optimizeElastic(
                elasticCustomers, List.of(depot), 1, 1, 1, true);
            require(elasticRoutes.size() == 3,
                    "Elastic planning should preserve the third route as virtual.");
            require(elasticRoutes.get(0).fleetTier == 0
                        && elasticRoutes.get(1).fleetTier == 1
                        && elasticRoutes.get(2).fleetTier == 2,
                    "JNI must preserve dedicated, ad-hoc, and virtual route tiers.");

            // A customer initially closest to an empty depot must be re-clustered
            // to a neighboring depot with spare dedicated capacity. Vehicles stay
            // at their home depot; only the customer assignment changes.
            Node emptyDepot = new Node(30, "D30", 0.0, 0.0, 0, true);
            emptyDepot.dedicatedVehicles = 0;
            emptyDepot.maxAdhocVehicles = 0;
            Node activeDepot = new Node(31, "D31", 10.0, 0.0, 0, true);
            activeDepot.dedicatedVehicles = 1;
            activeDepot.maxAdhocVehicles = 0;
            Node overflowCustomer = new Node(32, "C32", 1.0, 0.0, 1, false);
            List<Route> reassigned = bridge.optimizePerDepot(
                    List.of(overflowCustomer), List.of(emptyDepot, activeDepot), 5, true);
            require(reassigned.size() == 1,
                    "An empty depot's customer should be reassigned when another depot has spare real fleet.");
            require(reassigned.get(0).depot == activeDepot,
                    "Reassignment must change the customer cluster, not move the physical vehicle.");
            require(reassigned.get(0).fleetTier == 0,
                    "A feasible reassigned customer must use a real dedicated vehicle, not a virtual vehicle.");
            require(reassigned.get(0).stops.size() == 1
                        && reassigned.get(0).stops.get(0) == overflowCustomer,
                    "Re-clustering must preserve the customer exactly once.");

            // Capacity-aware initial assignment should move a border customer
            // toward a less-loaded depot instead of using distance alone.
            Node loadDepotA = new Node(40, "D40", 0.0, 0.0, 0, true);
            Node loadDepotB = new Node(41, "D41", 10.0, 0.0, 0, true);
            loadDepotA.dedicatedVehicles = 1;
            loadDepotA.maxAdhocVehicles = 0;
            loadDepotB.dedicatedVehicles = 1;
            loadDepotB.maxAdhocVehicles = 0;
            Node centralCustomer = new Node(42, "C42", 0.0, 0.0, 1, false);
            Node borderCustomer = new Node(43, "C43", 4.9, 0.0, 1, false);
            List<Route> balanced = bridge.optimizePerDepot(
                    List.of(centralCustomer, borderCustomer),
                    List.of(loadDepotA, loadDepotB), 2, true);
            Node borderAssignedDepot = null;
            for (Route route : balanced) {
                if (route.stops.contains(borderCustomer)) borderAssignedDepot = route.depot;
            }
            require(borderAssignedDepot == loadDepotB,
                    "A border customer should prefer the less-loaded depot when the load penalty outweighs the small distance difference.");
        } finally {
            bridge.close();
        }
        System.out.println("All RouteCraft JNI bridge tests passed.");
    }
}
