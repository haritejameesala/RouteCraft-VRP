import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

class Node {
    int id;
    int demand;
    double x;
    double y;
    String name;
    boolean depot;

    Node(int id, String name, double x, double y, int demand, boolean depot) {
        this.id = id;
        this.name = name;
        this.x = x;
        this.y = y;
        this.demand = demand;
        this.depot = depot;
    }

    String label() {
        return depot ? name : "C" + id;
    }
}

class Route {
    Node depot;
    List<Node> stops = new ArrayList<>();

    int load() {
        int totalDemand = 0;
        for (Node stop : stops) {
            totalDemand += stop.demand;
        }
        return totalDemand;
    }
}

class RoadNode {
    long id;
    double lat;
    double lon;
    double x;
    double y;
    List<RoadEdge> edges = new ArrayList<>();

    RoadNode(long id, double lat, double lon) {
        this.id = id;
        this.lat = lat;
        this.lon = lon;
    }
}

class RoadEdge {
    RoadNode to;
    double km;
    double minutes;
    double cost;

    RoadEdge(RoadNode to, double km, double minutes, double cost) {
        this.to = to;
        this.km = km;
        this.minutes = minutes;
        this.cost = cost;
    }
}

class Leg {
    List<RoadNode> path = new ArrayList<>();
    double km;
    double minutes;
    double cost;
    double score;
}

/**
 * JNI adapter between the Java interface and the C++ routing engine.
 */
class RouteOptimizerBridge implements AutoCloseable {
    private static final int METRIC_KILOMETRES = 0;
    private static final int METRIC_MINUTES = 1;
    private static final int METRIC_COST = 2;
    private static final int METRIC_SCORE = 3;

    static {
        System.loadLibrary("RouteCraftDsa");
    }

    private long nativeHandle;
    private final Map<Long, RoadNode> roadNodes;
    private final Map<String, Leg> legCache = new HashMap<>();

    RouteOptimizerBridge(Map<Long, RoadNode> roadNodes) {
        this.roadNodes = roadNodes;
        nativeHandle = nativeCreate();
    }

    void setWeights(double distance, double time, double cost) {
        nativeSetWeights(nativeHandle, distance, time, cost);
        legCache.clear();
    }

    void setRoadNetworkLoaded(boolean loaded) {
        nativeSetRoadNetworkLoaded(nativeHandle, loaded);
        legCache.clear();
    }

    void clearCache() {
        nativeClearCache(nativeHandle);
        legCache.clear();
    }

    /**
     * Sends the road graph to C++, keeps its largest strongly connected
     * component there, and removes discarded nodes from the Java map.
     */
    void setRoadGraph(Map<Long, RoadNode> graph) {
        List<RoadNode> nodes = new ArrayList<>(graph.values());
        Map<Long, Integer> nodeIndexes = new HashMap<>();
        long[] ids = new long[nodes.size()];
        double[] nodeValues = new double[nodes.size() * 4];

        for (int i = 0; i < nodes.size(); i++) {
            RoadNode node = nodes.get(i);
            ids[i] = node.id;
            nodeIndexes.put(node.id, i);
            int offset = i * 4;
            nodeValues[offset] = node.lat;
            nodeValues[offset + 1] = node.lon;
            nodeValues[offset + 2] = node.x;
            nodeValues[offset + 3] = node.y;
        }

        int edgeCount = 0;
        for (RoadNode node : nodes) {
            edgeCount += node.edges.size();
        }
        int[] edgeFrom = new int[edgeCount];
        int[] edgeTo = new int[edgeCount];
        double[] edgeValues = new double[edgeCount * 3];
        int edgeIndex = 0;
        for (RoadNode node : nodes) {
            Integer fromIndex = nodeIndexes.get(node.id);
            for (RoadEdge edge : node.edges) {
                Integer toIndex = nodeIndexes.get(edge.to.id);
                if (fromIndex == null || toIndex == null) {
                    continue;
                }
                edgeFrom[edgeIndex] = fromIndex;
                edgeTo[edgeIndex] = toIndex;
                int offset = edgeIndex * 3;
                edgeValues[offset] = edge.km;
                edgeValues[offset + 1] = edge.minutes;
                edgeValues[offset + 2] = edge.cost;
                edgeIndex++;
            }
        }

        if (edgeIndex != edgeCount) {
            edgeFrom = java.util.Arrays.copyOf(edgeFrom, edgeIndex);
            edgeTo = java.util.Arrays.copyOf(edgeTo, edgeIndex);
            edgeValues = java.util.Arrays.copyOf(edgeValues, edgeIndex * 3);
        }

        Set<Long> retainedIds = new HashSet<>();
        for (long id : nativeSetRoadGraph(nativeHandle, ids, nodeValues, edgeFrom, edgeTo, edgeValues)) {
            retainedIds.add(id);
        }
        graph.keySet().removeIf(id -> !retainedIds.contains(id));
        for (RoadNode node : graph.values()) {
            node.edges.removeIf(edge -> !graph.containsKey(edge.to.id));
        }
        legCache.clear();
    }

    List<Route> optimize(
            List<Node> customers,
            List<Node> depots,
            int vehicleCount,
            int capacity,
            boolean useSavings) {
        int customerCount = customers.size();
        int[] customerIds = new int[customerCount];
        int[] demands = new int[customerCount];
        double[] customerCoordinates = new double[customerCount * 2];
        for (int i = 0; i < customerCount; i++) {
            Node customer = customers.get(i);
            customerIds[i] = customer.id;
            demands[i] = customer.demand;
            customerCoordinates[i * 2] = customer.x;
            customerCoordinates[i * 2 + 1] = customer.y;
        }

        int depotCount = depots.size();
        int[] depotIds = new int[depotCount];
        double[] depotCoordinates = new double[depotCount * 2];
        String[] depotNames = new String[depotCount];
        for (int i = 0; i < depotCount; i++) {
            Node depot = depots.get(i);
            depotIds[i] = depot.id;
            depotCoordinates[i * 2] = depot.x;
            depotCoordinates[i * 2 + 1] = depot.y;
            depotNames[i] = depot.name;
        }

        int[][] result = nativeOptimize(
            nativeHandle,
            customerIds,
            demands,
            customerCoordinates,
            depotIds,
            depotCoordinates,
            depotNames,
            vehicleCount,
            capacity,
            useSavings);

        List<Route> routes = new ArrayList<>(result.length);
        for (int[] encodedRoute : result) {
            Route route = new Route();
            route.depot = depots.get(encodedRoute[0]);
            for (int i = 1; i < encodedRoute.length; i++) {
                route.stops.add(customers.get(encodedRoute[i]));
            }
            routes.add(route);
        }
        return routes;
    }

    double routeDistance(Node depot, List<Node> stops) {
        return routeMetric(depot, stops, METRIC_SCORE);
    }

    double routeKm(Node depot, List<Node> stops) {
        return routeMetric(depot, stops, METRIC_KILOMETRES);
    }

    double routeMinutes(Node depot, List<Node> stops) {
        return routeMetric(depot, stops, METRIC_MINUTES);
    }

    double routeCost(Node depot, List<Node> stops) {
        return routeMetric(depot, stops, METRIC_COST);
    }

    private double routeMetric(Node depot, List<Node> stops, int metric) {
        if (depot == null || stops.isEmpty()) {
            return 0.0;
        }

        int pointCount = stops.size() + 1;
        int[] ids = new int[pointCount];
        boolean[] depotFlags = new boolean[pointCount];
        double[] coordinates = new double[pointCount * 2];
        ids[0] = depot.id;
        depotFlags[0] = true;
        coordinates[0] = depot.x;
        coordinates[1] = depot.y;
        for (int i = 0; i < stops.size(); i++) {
            Node stop = stops.get(i);
            ids[i + 1] = stop.id;
            coordinates[(i + 1) * 2] = stop.x;
            coordinates[(i + 1) * 2 + 1] = stop.y;
        }
        return nativeRouteMetric(nativeHandle, ids, depotFlags, coordinates, metric);
    }

    Leg leg(Node from, Node to) {
        String key = from.id + ":" + (from.depot ? "D" : "C")
            + ":" + to.id + ":" + (to.depot ? "D" : "C");
        Leg cached = legCache.get(key);
        if (cached != null) {
            return cached;
        }

        Object[] nativeResult = nativeLeg(
            nativeHandle,
            from.id,
            from.depot,
            from.x,
            from.y,
            to.id,
            to.depot,
            to.x,
            to.y);
        double[] metrics = (double[]) nativeResult[0];
        long[] pathIds = (long[]) nativeResult[1];
        Leg result = new Leg();
        result.km = metrics[0];
        result.minutes = metrics[1];
        result.cost = metrics[2];
        result.score = metrics[3];
        for (long pathId : pathIds) {
            RoadNode roadNode = roadNodes.get(pathId);
            if (roadNode != null) {
                result.path.add(roadNode);
            }
        }
        legCache.put(key, result);
        return result;
    }

    RoadNode nearestRoadNode(double x, double y) {
        long id = nativeNearestRoadNode(nativeHandle, x, y);
        return roadNodes.get(id);
    }

    static double geoKm(double lat1, double lon1, double lat2, double lon2) {
        return nativeGeoKm(lat1, lon1, lat2, lon2);
    }

    @Override
    public synchronized void close() {
        if (nativeHandle != 0) {
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
        }
    }

    private static native long nativeCreate();
    private static native void nativeDestroy(long handle);
    private static native void nativeSetWeights(long handle, double distance, double time, double cost);
    private static native void nativeSetRoadNetworkLoaded(long handle, boolean loaded);
    private static native void nativeClearCache(long handle);
    private static native long[] nativeSetRoadGraph(
        long handle,
        long[] nodeIds,
        double[] nodeValues,
        int[] edgeFrom,
        int[] edgeTo,
        double[] edgeValues);
    private static native long nativeNearestRoadNode(long handle, double x, double y);
    private static native int[][] nativeOptimize(
        long handle,
        int[] customerIds,
        int[] customerDemands,
        double[] customerCoordinates,
        int[] depotIds,
        double[] depotCoordinates,
        String[] depotNames,
        int vehicleCount,
        int capacity,
        boolean useSavings);
    private static native double nativeRouteMetric(
        long handle,
        int[] nodeIds,
        boolean[] depotFlags,
        double[] coordinates,
        int metric);
    private static native Object[] nativeLeg(
        long handle,
        int fromId,
        boolean fromDepot,
        double fromX,
        double fromY,
        int toId,
        boolean toDepot,
        double toX,
        double toY);
    private static native double nativeGeoKm(double lat1, double lon1, double lat2, double lon2);
}
