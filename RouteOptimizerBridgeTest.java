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
        } finally {
            bridge.close();
        }
        System.out.println("All RouteCraft JNI bridge tests passed.");
    }
}
