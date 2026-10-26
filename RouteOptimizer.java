import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Deque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.PriorityQueue;
import java.util.Set;

/**
 * Data models and routing algorithms used by the RouteCraft interface.
 *
 * The optimizer assigns customers to depots, builds capacity-constrained
 * routes, and improves them with local-search heuristics. When an OSM road
 * graph is available, route legs are found with Dijkstra's algorithm.
 */
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
 * Keeps the route-planning logic independent from Swing and map rendering.
 */
class RouteOptimizer {
    private static final double TWO_OPT_EPSILON = 1e-9;
    private static final double THREE_OPT_EPSILON = 1e-8;

    private final Map<Long, RoadNode> roadNodes;
    private final Map<String, Leg> legCache = new HashMap<>();
    private double weightDistance = 0.40;
    private double weightTime = 0.35;
    private double weightCost = 0.25;
    private boolean roadNetworkLoaded;
    private int vehicleCount;
    private int capacity;

    RouteOptimizer(Map<Long, RoadNode> roadNodes) {
        this.roadNodes = roadNodes;
    }

    void setWeights(double distance, double time, double cost) {
        if (weightDistance != distance || weightTime != time || weightCost != cost) {
            weightDistance = distance;
            weightTime = time;
            weightCost = cost;
            clearCache();
        }
    }

    void clearCache() {
        legCache.clear();
    }

    void setRoadNetworkLoaded(boolean loaded) {
        if (roadNetworkLoaded != loaded) {
            roadNetworkLoaded = loaded;
            clearCache();
        }
    }

    /**
     * Builds routes using the selected heuristic, then improves each route
     * with 2-opt and a bounded 3-opt pass.
     */
    List<Route> optimize(
            List<Node> customers,
            List<Node> depots,
            int vehicleCount,
            int capacity,
            boolean useSavings) {
        this.vehicleCount = vehicleCount;
        this.capacity = capacity;
        clearCache();

        List<Route> routes = useSavings
                ? savingsMultiDepot(customers, depots)
                : nearestMultiDepot(customers, depots);

        for (Route route : routes) {
            twoOpt(route);
            threeOptLite(route);
        }
        routes.removeIf(route -> route.stops.isEmpty());
        return routes;
    }

    double routeDistance(Node depot, List<Node> stops) {
        return routeMetric(depot, stops, Metric.SCORE);
    }

    double routeKm(Node depot, List<Node> stops) {
        return routeMetric(depot, stops, Metric.DISTANCE);
    }

    double routeMinutes(Node depot, List<Node> stops) {
        return routeMetric(depot, stops, Metric.TIME);
    }

    double routeCost(Node depot, List<Node> stops) {
        return routeMetric(depot, stops, Metric.COST);
    }

    Leg leg(Node from, Node to) {
        String key = from.id + ":" + (from.depot ? "D" : "C")
                + ":" + to.id + ":" + (to.depot ? "D" : "C");
        Leg cached = legCache.get(key);
        if (cached != null) {
            return cached;
        }

        Leg result = new Leg();
        if (roadNetworkLoaded && !roadNodes.isEmpty()) {
            RoadNode start = nearestRoadNode(from.x, from.y);
            RoadNode end = nearestRoadNode(to.x, to.y);
            if (start != null && end != null) {
                result = shortestLeg(start, end);
                if (result != null) {
                    legCache.put(key, result);
                    return result;
                }
            }
        }

        double straightLineDistance = Math.hypot(from.x - to.x, from.y - to.y);
        result.km = straightLineDistance;
        result.minutes = straightLineDistance * 2.0;
        result.cost = straightLineDistance * 12.0;
        result.score = objectiveScore(result.km, result.minutes, result.cost);
        legCache.put(key, result);
        return result;
    }

    RoadNode nearestRoadNode(double x, double y) {
        RoadNode nearest = null;
        double bestDistance = Double.MAX_VALUE;
        for (RoadNode node : roadNodes.values()) {
            double distance = Math.hypot(node.x - x, node.y - y);
            if (distance < bestDistance) {
                bestDistance = distance;
                nearest = node;
            }
        }
        return nearest;
    }

    private Map<Node, List<Node>> assignCustomersToDepots(
            List<Node> customers,
            List<Node> depots) {
        Map<Node, List<Node>> groups = new LinkedHashMap<>();
        for (Node depot : depots) {
            groups.put(depot, new ArrayList<>());
        }
        for (Node customer : customers) {
            Node closestDepot = depots.get(0);
            for (Node depot : depots) {
                if (distance(customer, depot) < distance(customer, closestDepot)) {
                    closestDepot = depot;
                }
            }
            groups.get(closestDepot).add(customer);
        }
        return groups;
    }

    /**
     * Creates one route at a time, repeatedly choosing the nearest customer
     * that fits in the vehicle's remaining capacity.
     */
    private List<Route> nearestMultiDepot(List<Node> customers, List<Node> depots) {
        Map<Node, List<Node>> groups = assignCustomersToDepots(customers, depots);
        List<Route> routes = new ArrayList<>();

        for (Node depot : depots) {
            List<Node> remaining = new ArrayList<>(groups.get(depot));
            while (!remaining.isEmpty() && routes.size() < vehicleCount) {
                Route route = new Route();
                route.depot = depot;
                Node current = depot;

                while (true) {
                    Node nearest = null;
                    double bestDistance = Double.MAX_VALUE;
                    for (Node customer : remaining) {
                        if (route.load() + customer.demand <= capacity
                                && distance(current, customer) < bestDistance) {
                            nearest = customer;
                            bestDistance = distance(current, customer);
                        }
                    }
                    if (nearest == null) {
                        break;
                    }
                    route.stops.add(nearest);
                    remaining.remove(nearest);
                    current = nearest;
                }

                if (!route.stops.isEmpty()) {
                    routes.add(route);
                }
            }
        }
        return routes;
    }

    /**
     * Clarke-Wright savings: merge route ends when the merge saves travel
     * score and stays within capacity, then reduce routes to the fleet limit.
     */
    private List<Route> savingsMultiDepot(List<Node> customers, List<Node> depots) {
        Map<Node, List<Node>> groups = assignCustomersToDepots(customers, depots);
        List<Route> routes = new ArrayList<>();

        for (Node depot : depots) {
            List<Node> group = groups.get(depot);
            List<Route> depotRoutes = new ArrayList<>();
            Map<Integer, Route> owner = new HashMap<>();
            for (Node customer : group) {
                Route route = new Route();
                route.depot = depot;
                route.stops.add(customer);
                depotRoutes.add(route);
                owner.put(customer.id, route);
            }

            List<Saving> savings = new ArrayList<>();
            for (int i = 0; i < group.size(); i++) {
                for (int j = i + 1; j < group.size(); j++) {
                    Node first = group.get(i);
                    Node second = group.get(j);
                    double saving = distance(depot, first) + distance(depot, second)
                            - distance(first, second);
                    savings.add(new Saving(first, second, saving));
                }
            }
            savings.sort((first, second) -> Double.compare(second.value, first.value));

            for (Saving saving : savings) {
                Route firstRoute = owner.get(saving.first.id);
                Route secondRoute = owner.get(saving.second.id);
                if (firstRoute == secondRoute || firstRoute == null || secondRoute == null
                        || firstRoute.load() + secondRoute.load() > capacity) {
                    continue;
                }

                List<Node> merged = mergeAtRouteEnds(firstRoute, secondRoute, saving);
                if (merged != null) {
                    firstRoute.stops = merged;
                    for (Node stop : secondRoute.stops) {
                        owner.put(stop.id, firstRoute);
                    }
                    depotRoutes.remove(secondRoute);
                }
            }
            routes.addAll(depotRoutes);
        }

        while (routes.size() > vehicleCount) {
            MergeCandidate best = findBestRouteMerge(routes, depots);
            if (best == null) {
                break;
            }
            Route merged = routes.get(best.firstIndex);
            merged.stops = best.stops;
            merged.depot = best.depot;
            routes.remove(best.secondIndex);
        }
        return routes;
    }

    private List<Node> mergeAtRouteEnds(Route first, Route second, Saving saving) {
        Node firstStop = saving.first;
        Node secondStop = saving.second;
        boolean firstAtEnd = first.stops.get(first.stops.size() - 1) == firstStop;
        boolean firstAtStart = first.stops.get(0) == firstStop;
        boolean secondAtEnd = second.stops.get(second.stops.size() - 1) == secondStop;
        boolean secondAtStart = second.stops.get(0) == secondStop;

        List<Node> merged;
        if (firstAtEnd && secondAtStart) {
            merged = new ArrayList<>(first.stops);
            merged.addAll(second.stops);
        } else if (firstAtStart && secondAtEnd) {
            merged = new ArrayList<>(second.stops);
            merged.addAll(first.stops);
        } else if (firstAtStart && secondAtStart) {
            merged = new ArrayList<>(first.stops);
            Collections.reverse(merged);
            merged.addAll(second.stops);
        } else if (firstAtEnd && secondAtEnd) {
            merged = new ArrayList<>(first.stops);
            List<Node> reversed = new ArrayList<>(second.stops);
            Collections.reverse(reversed);
            merged.addAll(reversed);
        } else {
            return null;
        }
        return merged;
    }

    private MergeCandidate findBestRouteMerge(List<Route> routes, List<Node> depots) {
        double bestScore = Double.MAX_VALUE;
        MergeCandidate best = null;

        for (int i = 0; i < routes.size(); i++) {
            for (int j = i + 1; j < routes.size(); j++) {
                Route first = routes.get(i);
                Route second = routes.get(j);
                if (first.load() + second.load() > capacity) {
                    continue;
                }

                for (int reversal = 0; reversal < 4; reversal++) {
                    List<Node> firstStops = new ArrayList<>(first.stops);
                    List<Node> secondStops = new ArrayList<>(second.stops);
                    if ((reversal & 1) != 0) {
                        Collections.reverse(firstStops);
                    }
                    if ((reversal & 2) != 0) {
                        Collections.reverse(secondStops);
                    }
                    List<Node> merged = new ArrayList<>(firstStops);
                    merged.addAll(secondStops);

                    for (Node depot : depots) {
                        double score = routeDistance(depot, merged);
                        if (score < bestScore) {
                            bestScore = score;
                            best = new MergeCandidate(i, j, depot, merged);
                        }
                    }
                }
            }
        }
        return best;
    }

    /**
     * 2-opt removes two edges and reverses the segment between them whenever
     * that reduces the route's weighted travel score.
     */
    private void twoOpt(Route route) {
        boolean improved = true;
        int iterations = 0;
        while (improved && iterations++ < 100) {
            improved = false;
            for (int i = 0; i < route.stops.size() - 1; i++) {
                for (int k = i + 1; k < route.stops.size(); k++) {
                    Node before = i == 0 ? route.depot : route.stops.get(i - 1);
                    Node first = route.stops.get(i);
                    Node last = route.stops.get(k);
                    Node after = k == route.stops.size() - 1
                            ? route.depot
                            : route.stops.get(k + 1);

                    double currentScore = distance(before, first) + distance(last, after);
                    double improvedScore = distance(before, last) + distance(first, after);
                    if (improvedScore + TWO_OPT_EPSILON < currentScore) {
                        Collections.reverse(route.stops.subList(i, k + 1));
                        improved = true;
                    }
                }
            }
        }
    }

    /**
     * Tries three segment reversals per pass; the small round limit keeps the
     * local search practical for an interactive desktop application.
     */
    private void threeOptLite(Route route) {
        int stopCount = route.stops.size();
        if (stopCount < 5) {
            return;
        }

        boolean improved = true;
        int rounds = 0;
        while (improved && rounds++ < 2) {
            improved = false;
            search:
            for (int i = 0; i < stopCount - 3; i++) {
                for (int j = i + 1; j < stopCount - 2; j++) {
                    for (int k = j + 1; k < stopCount - 1; k++) {
                        List<Node> original = new ArrayList<>(route.stops);
                        List<Node> bestStops = original;
                        double bestDistance = routeDistance(route.depot, original);

                        List<Node> candidate = new ArrayList<>(original);
                        Collections.reverse(candidate.subList(i, j));
                        double candidateDistance = routeDistance(route.depot, candidate);
                        if (candidateDistance + THREE_OPT_EPSILON < bestDistance) {
                            bestDistance = candidateDistance;
                            bestStops = candidate;
                        }

                        candidate = new ArrayList<>(original);
                        Collections.reverse(candidate.subList(j, k));
                        candidateDistance = routeDistance(route.depot, candidate);
                        if (candidateDistance + THREE_OPT_EPSILON < bestDistance) {
                            bestDistance = candidateDistance;
                            bestStops = candidate;
                        }

                        candidate = new ArrayList<>(original);
                        Collections.reverse(candidate.subList(i, k));
                        candidateDistance = routeDistance(route.depot, candidate);
                        if (candidateDistance + THREE_OPT_EPSILON < bestDistance) {
                            bestDistance = candidateDistance;
                            bestStops = candidate;
                        }

                        if (bestStops != original) {
                            route.stops = bestStops;
                            improved = true;
                            break search;
                        }
                    }
                }
            }
        }
    }

    private double distance(Node first, Node second) {
        return leg(first, second).score;
    }

    private double routeMetric(Node depot, List<Node> stops, Metric metric) {
        if (depot == null || stops.isEmpty()) {
            return 0;
        }

        double total = metricValue(leg(depot, stops.get(0)), metric);
        for (int i = 1; i < stops.size(); i++) {
            total += metricValue(leg(stops.get(i - 1), stops.get(i)), metric);
        }
        total += metricValue(leg(stops.get(stops.size() - 1), depot), metric);
        return total;
    }

    private double metricValue(Leg leg, Metric metric) {
        switch (metric) {
            case DISTANCE:
                return leg.km;
            case TIME:
                return leg.minutes;
            case COST:
                return leg.cost;
            case SCORE:
            default:
                return leg.score;
        }
    }

    private double objectiveScore(double km, double minutes, double cost) {
        return weightDistance * km
                + weightTime * (minutes / 60.0 * 30.0)
                + weightCost * (cost / 12.0);
    }

    /**
     * Dijkstra's algorithm finds the minimum weighted-cost path through the
     * road graph. The priority queue always expands the lowest known score.
     */
    private Leg shortestLeg(RoadNode start, RoadNode target) {
        Leg result = new Leg();
        if (start == target) {
            result.path.add(start);
            return result;
        }

        Map<RoadNode, Double> distances = new HashMap<>();
        Map<RoadNode, RoadNode> previous = new HashMap<>();
        PriorityQueue<RoadNode> queue = new PriorityQueue<>(
                Comparator.comparingDouble(
                        node -> distances.getOrDefault(node, Double.POSITIVE_INFINITY)));
        Set<RoadNode> settled = new HashSet<>();
        distances.put(start, 0.0);
        queue.add(start);

        while (!queue.isEmpty()) {
            RoadNode current = queue.poll();
            if (!settled.add(current)) {
                continue;
            }
            if (current == target) {
                break;
            }

            double currentDistance = distances.get(current);
            for (RoadEdge edge : current.edges) {
                double candidateDistance = currentDistance
                        + objectiveScore(edge.km, edge.minutes, edge.cost);
                if (candidateDistance < distances.getOrDefault(
                        edge.to, Double.POSITIVE_INFINITY)) {
                    distances.put(edge.to, candidateDistance);
                    previous.put(edge.to, current);
                    queue.remove(edge.to);
                    queue.add(edge.to);
                }
            }
        }

        if (!distances.containsKey(target)) {
            // A straight connector keeps the route usable if the graph is disconnected.
            result.km = geoKm(start.lat, start.lon, target.lat, target.lon);
            result.minutes = result.km * 2.0;
            result.cost = result.km * 12.0;
            result.score = objectiveScore(result.km, result.minutes, result.cost);
            result.path.add(start);
            result.path.add(target);
            return result;
        }

        List<RoadNode> reversedPath = new ArrayList<>();
        RoadNode current = target;
        while (current != null) {
            reversedPath.add(current);
            if (current == start) {
                break;
            }
            current = previous.get(current);
        }
        Collections.reverse(reversedPath);
        result.path.addAll(reversedPath);

        for (int i = 0; i < reversedPath.size() - 1; i++) {
            RoadNode from = reversedPath.get(i);
            RoadNode to = reversedPath.get(i + 1);
            for (RoadEdge edge : from.edges) {
                if (edge.to == to) {
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

    /**
     * Keeps only the largest strongly connected component using Kosaraju's
     * two-pass graph algorithm, then removes edges to discarded nodes.
     */
    static void retainLargestStronglyConnectedComponent(Set<RoadNode> used) {
        List<RoadNode> nodes = new ArrayList<>(used);
        Map<RoadNode, List<RoadNode>> reverseGraph = new HashMap<>();
        for (RoadNode node : nodes) {
            for (RoadEdge edge : node.edges) {
                reverseGraph.computeIfAbsent(edge.to, ignored -> new ArrayList<>()).add(node);
            }
        }

        Set<RoadNode> visited = new HashSet<>();
        Deque<RoadNode> finishingOrder = new ArrayDeque<>();
        for (RoadNode root : nodes) {
            if (!visited.add(root)) {
                continue;
            }
            Deque<TraversalFrame> stack = new ArrayDeque<>();
            stack.push(new TraversalFrame(root));
            while (!stack.isEmpty()) {
                TraversalFrame frame = stack.peek();
                if (frame.nextEdge < frame.node.edges.size()) {
                    RoadNode next = frame.node.edges.get(frame.nextEdge++).to;
                    if (used.contains(next) && visited.add(next)) {
                        stack.push(new TraversalFrame(next));
                    }
                } else {
                    stack.pop();
                    finishingOrder.push(frame.node);
                }
            }
        }

        Set<RoadNode> assigned = new HashSet<>();
        Set<RoadNode> largestComponent = new HashSet<>();
        for (RoadNode root : finishingOrder) {
            if (!assigned.add(root)) {
                continue;
            }
            Set<RoadNode> component = new HashSet<>();
            Deque<RoadNode> stack = new ArrayDeque<>();
            stack.push(root);
            while (!stack.isEmpty()) {
                RoadNode node = stack.pop();
                component.add(node);
                for (RoadNode previous : reverseGraph.getOrDefault(node, Collections.emptyList())) {
                    if (assigned.add(previous)) {
                        stack.push(previous);
                    }
                }
            }
            if (component.size() > largestComponent.size()) {
                largestComponent = component;
            }
        }

        used.retainAll(largestComponent);
        for (RoadNode node : used) {
            node.edges.removeIf(edge -> !used.contains(edge.to));
        }
    }

    static double geoKm(double lat1, double lon1, double lat2, double lon2) {
        double earthRadiusKm = 6371.0;
        double latitudeDelta = Math.toRadians(lat2 - lat1);
        double longitudeDelta = Math.toRadians(lon2 - lon1);
        double a = Math.sin(latitudeDelta / 2) * Math.sin(latitudeDelta / 2)
                + Math.cos(Math.toRadians(lat1)) * Math.cos(Math.toRadians(lat2))
                * Math.sin(longitudeDelta / 2) * Math.sin(longitudeDelta / 2);
        return 2 * earthRadiusKm * Math.asin(Math.sqrt(a));
    }

    private enum Metric {
        DISTANCE,
        TIME,
        COST,
        SCORE
    }

    private static class Saving {
        final Node first;
        final Node second;
        final double value;

        Saving(Node first, Node second, double value) {
            this.first = first;
            this.second = second;
            this.value = value;
        }
    }

    private static class MergeCandidate {
        final int firstIndex;
        final int secondIndex;
        final Node depot;
        final List<Node> stops;

        MergeCandidate(int firstIndex, int secondIndex, Node depot, List<Node> stops) {
            this.firstIndex = firstIndex;
            this.secondIndex = secondIndex;
            this.depot = depot;
            this.stops = stops;
        }
    }

    private static class TraversalFrame {
        final RoadNode node;
        int nextEdge;

        TraversalFrame(RoadNode node) {
            this.node = node;
        }
    }
}
