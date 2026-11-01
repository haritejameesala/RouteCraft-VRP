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
    String initialDepotName;
    int dedicatedVehicles = 2;
    int maxAdhocVehicles = 1;

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
    int fleetTier; // 0 dedicated, 1 ad-hoc, 2 virtual (not dispatchable)

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
            List<Node> customers, List<Node> depots, int vehicleCount,
            int capacity, boolean useSavings) {
        return optimizeElastic(customers, depots, vehicleCount, 0, capacity, useSavings);
    }

    // Hybrid MDVRP with capacity-aware assignment and one bounded feedback pass.
    // Physical vehicles remain at their home depots; only customer clusters change.
    private static final double DEPOT_LOAD_ALPHA = 0.5;
    private static final double ADHOC_ROUTE_PENALTY = 100.0;
    private static final double VIRTUAL_DEMAND_PENALTY = 100000.0;

    List<Route> optimizePerDepot(List<Node> customers, List<Node> depots,
            int capacity, boolean useSavings) {
        if (depots.isEmpty() && !customers.isEmpty()) {
            throw new IllegalArgumentException("At least one depot is required.");
        }
        if (capacity <= 0 && !customers.isEmpty()) {
            throw new IllegalArgumentException("Vehicle capacity must be positive.");
        }
        if (customers.isEmpty()) return new ArrayList<>();

        List<List<Node>> firstClusters = capacityAwareInitialClusters(
                customers, depots, capacity, true);
        List<Route> firstPass = runDepotPipeline(firstClusters, depots, capacity, useSavings);

        // Feedback pass: move selected customers from depots that needed ad-hoc
        // or virtual routes to alternative depots, then rerun the same pipeline.
        List<List<Node>> alternativeClusters = copyClusters(firstClusters);
        boolean changed = applyBottleneckFeedback(alternativeClusters, firstPass,
                depots, capacity);
        if (!changed) return firstPass;

        List<Route> secondPass = runDepotPipeline(alternativeClusters, depots,
                capacity, useSavings);
        return solutionObjective(secondPass) + 1e-9 < solutionObjective(firstPass)
                ? secondPass : firstPass;
    }

    private List<List<Node>> capacityAwareInitialClusters(List<Node> customers,
            List<Node> depots, int capacity, boolean recordInitialDepot) {
        List<List<Node>> clusters = new ArrayList<>();
        double[] assignedDemand = new double[depots.size()];
        for (int i = 0; i < depots.size(); i++) clusters.add(new ArrayList<>());

        for (Node customer : customers) {
            int best = 0;
            double bestCost = Double.POSITIVE_INFINITY;
            for (int i = 0; i < depots.size(); i++) {
                Node depot = depots.get(i);
                double dx = customer.x - depot.x;
                double dy = customer.y - depot.y;
                double distance = Math.hypot(dx, dy);
                double totalCapacity = (double) (depot.dedicatedVehicles
                        + depot.maxAdhocVehicles) * capacity;
                double utilization = totalCapacity > 0.0
                        ? assignedDemand[i] / totalCapacity : 1.0;
                // A zero-capacity depot is strongly discouraged, but remains a
                // fallback when every depot has zero configured fleet.
                double loadFactor = totalCapacity > 0.0
                        ? 1.0 + DEPOT_LOAD_ALPHA * utilization : 1.0e6;
                double assignmentCost = distance * loadFactor;
                if (assignmentCost < bestCost) {
                    bestCost = assignmentCost;
                    best = i;
                }
            }
            clusters.get(best).add(customer);
            assignedDemand[best] += customer.demand;
            if (recordInitialDepot) customer.initialDepotName = depots.get(best).name;
        }
        return clusters;
    }

    private List<List<Node>> copyClusters(List<List<Node>> source) {
        List<List<Node>> copy = new ArrayList<>();
        for (List<Node> cluster : source) copy.add(new ArrayList<>(cluster));
        return copy;
    }

    private List<Route> runDepotPipeline(List<List<Node>> sourceClusters,
            List<Node> depots, int capacity, boolean useSavings) {
        List<List<Node>> clusters = copyClusters(sourceClusters);
        // Stage 1: dedicated fleet network-wide, then customer re-clustering.
        List<List<Route>> dedicatedPlans = buildDepotPlans(clusters, depots,
                capacity, useSavings, true);
        reassignVirtualCustomers(clusters, dedicatedPlans, depots, capacity,
                useSavings, true);

        // Stage 2: local ad-hoc activation, followed by another attempt to use
        // remaining real capacity elsewhere. Vehicles themselves never move.
        List<List<Route>> fullPlans = buildDepotPlans(clusters, depots,
                capacity, useSavings, false);
        reassignVirtualCustomers(clusters, fullPlans, depots, capacity,
                useSavings, false);
        fullPlans = buildDepotPlans(clusters, depots, capacity, useSavings, false);

        List<Route> result = new ArrayList<>();
        for (List<Route> depotRoutes : fullPlans) result.addAll(depotRoutes);
        return result;
    }

    private boolean applyBottleneckFeedback(List<List<Node>> clusters,
            List<Route> routes, List<Node> depots, int capacity) {

        // Candidate customers are taken from ad-hoc/virtual routes, prioritizing
        // virtual demand and then customers farthest from their current depot.
        List<Node> candidates = new ArrayList<>();
        for (Route route : routes) {
            if (route.fleetTier == 0) continue;
            for (Node customer : route.stops) {
                if (!candidates.contains(customer)) candidates.add(customer);
            }
        }
        if (candidates.isEmpty()) return false;
        candidates.sort((a, b) -> {
            int tierA = customerTier(a, routes), tierB = customerTier(b, routes);
            if (tierA != tierB) return Integer.compare(tierB, tierA);
            return Double.compare(distanceFromAssignedDepot(b, routes),
                    distanceFromAssignedDepot(a, routes));
        });

        int moveLimit = Math.max(1, (candidates.size() + 2) / 3);
        boolean changed = false;
        for (int c = 0; c < Math.min(moveLimit, candidates.size()); c++) {
            Node customer = candidates.get(c);
            int source = -1;
            for (int i = 0; i < clusters.size(); i++) {
                if (clusters.get(i).contains(customer)) { source = i; break; }
            }
            if (source < 0) continue;

            int target = -1;
            double bestCost = Double.POSITIVE_INFINITY;
            double[] loads = new double[depots.size()];
            for (int i = 0; i < clusters.size(); i++) {
                for (Node assigned : clusters.get(i)) loads[i] += assigned.demand;
            }
            for (int i = 0; i < depots.size(); i++) {
                if (i == source) continue;
                Node depot = depots.get(i);
                double totalCapacity = (double) (depot.dedicatedVehicles
                        + depot.maxAdhocVehicles) * capacity;
                if (totalCapacity <= 0.0 || loads[i] + customer.demand > totalCapacity) continue;
                double distance = Math.hypot(customer.x - depot.x, customer.y - depot.y);
                double cost = distance * (1.0 + DEPOT_LOAD_ALPHA
                        * (loads[i] / totalCapacity));
                if (cost < bestCost) { bestCost = cost; target = i; }
            }
            if (target >= 0) {
                clusters.get(source).remove(customer);
                clusters.get(target).add(customer);
                changed = true;
            }
        }
        return changed;
    }

    private int customerTier(Node customer, List<Route> routes) {
        int tier = 0;
        for (Route route : routes) if (route.stops.contains(customer))
            tier = Math.max(tier, route.fleetTier);
        return tier;
    }

    private double distanceFromAssignedDepot(Node customer, List<Route> routes) {
        for (Route route : routes) if (route.stops.contains(customer))
            return Math.hypot(customer.x - route.depot.x, customer.y - route.depot.y);
        return 0.0;
    }

    private double solutionObjective(List<Route> routes) {
        double objective = 0.0;
        for (Route route : routes) {
            objective += routeDistance(route.depot, route.stops);
            if (route.fleetTier == 1) objective += ADHOC_ROUTE_PENALTY;
            if (route.fleetTier == 2) {
                objective += VIRTUAL_DEMAND_PENALTY * Math.max(1, route.load());
            }
        }
        return objective;
    }

    private List<List<Route>> buildDepotPlans(List<List<Node>> clusters,
            List<Node> depots, int capacity, boolean useSavings,
            boolean dedicatedOnly) {
        List<List<Route>> plans = new ArrayList<>();
        for (int i = 0; i < depots.size(); i++) {
            Node depot = depots.get(i);
            int dedicated = depot.dedicatedVehicles;
            int adhoc = dedicatedOnly ? 0 : depot.maxAdhocVehicles;
            plans.add(optimizeElastic(clusters.get(i),
                    java.util.Collections.singletonList(depot), dedicated,
                    adhoc, capacity, useSavings));
        }
        return plans;
    }

    private boolean hasVirtualRoutes(List<Route> routes) {
        for (Route route : routes) if (route.fleetTier == 2) return true;
        return false;
    }

    private double planScore(List<Route> routes) {
        double total = 0.0;
        for (Route route : routes) total += routeDistance(route.depot, route.stops);
        return total;
    }

    // Greedily moves only customers currently assigned to virtual routes. A
    // candidate receiving depot is accepted only when its own real fleet can
    // serve the entire resulting cluster. The best feasible incremental route
    // score is selected, and both affected depot plans are recalculated.
    private void reassignVirtualCustomers(List<List<Node>> clusters,
            List<List<Route>> plans, List<Node> depots, int capacity,
            boolean useSavings, boolean dedicatedOnly) {
        int maxMoves = clusters.stream().mapToInt(List::size).sum();
        for (int move = 0; move < maxMoves; move++) {
            int sourceIndex = -1;
            Node overflowCustomer = null;
            double farthestFromHome = -1.0;

            // Prefer moving edge customers first, leaving the source depot's
            // geographically central customers in its original cluster.
            for (int i = 0; i < plans.size(); i++) {
                for (Route route : plans.get(i)) {
                    if (route.fleetTier != 2) continue;
                    for (Node customer : route.stops) {
                        Node source = depots.get(i);
                        double dx = customer.x - source.x, dy = customer.y - source.y;
                        double distance = dx * dx + dy * dy;
                        if (distance > farthestFromHome) {
                            farthestFromHome = distance;
                            sourceIndex = i;
                            overflowCustomer = customer;
                        }
                    }
                }
            }
            if (overflowCustomer == null) return;

            int bestTarget = -1;
            double bestIncrement = Double.POSITIVE_INFINITY;
            for (int target = 0; target < depots.size(); target++) {
                if (target == sourceIndex) continue;
                Node depot = depots.get(target);
                int dedicated = depot.dedicatedVehicles;
                int adhoc = dedicatedOnly ? 0 : depot.maxAdhocVehicles;
                if (dedicated + adhoc == 0) continue;

                List<Node> candidateCluster = new ArrayList<>(clusters.get(target));
                candidateCluster.add(overflowCustomer);
                List<Route> candidatePlan = optimizeElastic(candidateCluster,
                        java.util.Collections.singletonList(depot), dedicated,
                        adhoc, capacity, useSavings);
                if (hasVirtualRoutes(candidatePlan)) continue;

                double increment = planScore(candidatePlan) - planScore(plans.get(target));
                // A small proximity tie-breaker makes otherwise equal choices
                // favor the geographically nearer receiving depot.
                double dx = overflowCustomer.x - depot.x;
                double dy = overflowCustomer.y - depot.y;
                increment += Math.hypot(dx, dy) * 1e-6;
                if (increment < bestIncrement) {
                    bestIncrement = increment;
                    bestTarget = target;
                }
            }

            if (bestTarget < 0) return; // No real depot can absorb this customer.

            clusters.get(sourceIndex).remove(overflowCustomer);
            clusters.get(bestTarget).add(overflowCustomer);
            Node sourceDepot = depots.get(sourceIndex);
            Node targetDepot = depots.get(bestTarget);
            plans.set(sourceIndex, optimizeElastic(clusters.get(sourceIndex),
                    java.util.Collections.singletonList(sourceDepot),
                    sourceDepot.dedicatedVehicles,
                    dedicatedOnly ? 0 : sourceDepot.maxAdhocVehicles,
                    capacity, useSavings));
            plans.set(bestTarget, optimizeElastic(clusters.get(bestTarget),
                    java.util.Collections.singletonList(targetDepot),
                    targetDepot.dedicatedVehicles,
                    dedicatedOnly ? 0 : targetDepot.maxAdhocVehicles,
                    capacity, useSavings));
        }
    }

    List<Route> optimizeElastic(
            List<Node> customers, List<Node> depots, int dedicatedVehicles,
            int maxAdhocVehicles, int capacity, boolean useSavings) {
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
            dedicatedVehicles,
            maxAdhocVehicles,
            capacity,
            useSavings);

        List<Route> routes = new ArrayList<>(result.length);
        for (int[] encodedRoute : result) {
            Route route = new Route();
            route.depot = depots.get(encodedRoute[0]);
            route.fleetTier = encodedRoute[1];
            for (int i = 2; i < encodedRoute.length; i++) {
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
        int dedicatedVehicles,
        int maxAdhocVehicles,
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
