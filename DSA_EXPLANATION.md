# RouteCraft DSA Explanation

This guide explains the routing algorithms used by RouteCraft. The Java
implementation is in [`RouteOptimizer.java`](./RouteOptimizer.java); the
Swing interface calls it to calculate routes and display the results.

## Problem being solved

RouteCraft solves a capacitated, multi-depot Vehicle Routing Problem (VRP):

- Customers have locations and delivery demands.
- Depots are the starting and ending points for vehicles.
- Each vehicle has a maximum carrying capacity.
- The available fleet has a maximum number of vehicles.
- The goal is to build practical routes with a low distance, time, and
  operating-cost score.

The problem is computationally difficult to solve exactly for large inputs.
RouteCraft therefore uses heuristics: algorithms that find good practical
solutions without guaranteeing the global optimum.

## Data structures

| Type | Purpose |
| --- | --- |
| `Node` | Represents a customer or depot. A customer has a delivery demand; a depot has none. |
| `Route` | Stores one vehicle's depot and ordered customer stops. Its `load()` method sums the stop demands. |
| `RoadNode` | Represents an intersection in the imported road graph and stores its outgoing edges. |
| `RoadEdge` | Represents a directed road connection and its distance, estimated travel time, and cost. |
| `Leg` | Stores a calculated path between two customer/depot locations and its measurements. |
| `Map` | Looks up road nodes, best-known shortest-path distances, predecessors, and cached route legs. |
| `PriorityQueue` | Selects the next closest road node while Dijkstra's algorithm searches. |
| `Set` and `Deque` | Track visited graph nodes and manage the iterative Kosaraju graph traversal. |

## Optimization flow

The `optimize(...)` method coordinates the main steps:

1. Split customers into groups by their nearest depot.
2. Build initial routes using the selected construction heuristic:
   nearest neighbor or Clarke-Wright savings.
3. Keep route loads within the vehicle capacity during construction and keep
   the result within the fleet limit where feasible.
4. Improve each route with 2-opt, followed by a bounded 3-opt pass.
5. Return the non-empty routes to the application for display.

Each route is a closed trip: it starts at its depot, visits its customer
stops in order, and returns to the same depot.

## Route construction algorithms

### Nearest neighbor

For each depot, the algorithm repeatedly starts a vehicle route at the depot.
It chooses the closest remaining customer that fits in the vehicle's
remaining capacity, adds that customer to the route, and continues from that
customer. The process stops when no remaining customer fits or the fleet limit
is reached.

This is easy to follow and tends to make locally short steps. A locally close
choice is not always part of the best overall route.

### Clarke-Wright savings

The algorithm first creates one short route per customer. For every customer
pair `i` and `j`, it calculates the saving from joining their routes:

```text
saving(i, j) = distance(depot, i)
             + distance(depot, j)
             - distance(i, j)
```

It sorts the pairs from largest saving to smallest. In that order, it merges
two routes if the customers are at compatible route ends and their combined
delivery demand does not exceed vehicle capacity. If there are still more
routes than vehicles, it searches for the lowest-score feasible route merges.

The saving is measured using the project's weighted travel score. This
heuristic often combines nearby deliveries into fewer routes than building
each route independently.

## Route improvement algorithms

### 2-opt

2-opt looks at two connections in a route. It removes them, reverses the
section of customer stops between them, and keeps the change when it lowers
the route score. It repeats the search until no improvement is found or the
100-iteration limit is reached.

The route is checked as a round trip, so its depot connections are included
when comparing changes.

### Bounded 3-opt

The 3-opt pass tries three different segment reversals for selected triples
of stop positions. It keeps a better ordering and repeats for at most two
rounds. The limit keeps the extra search manageable for interactive use.

Both improvement steps are heuristics; they can improve a constructed route
but do not prove that it is optimal.

## Road-network algorithms

### Dijkstra's shortest path

When an OpenStreetMap road graph is loaded, customer and depot locations are
matched to nearby road nodes. Dijkstra's algorithm then searches the directed
road graph for a path with the lowest weighted score. A priority queue
selects the unsettled road node with the smallest known score. The algorithm
relaxes its outgoing edges and records each node's predecessor so it can
reconstruct the path.

The edge score combines distance, travel time, and cost using the user's
weights:

```text
score = distanceWeight * distance
      + timeWeight * (minutes / 60 * 30)
      + costWeight * (cost / 12)
```

The weights are normalized by the interface. If there is no road graph, the
optimizer uses a straight-line distance estimate instead.

### Kosaraju's strongly connected component search

Before routing on imported roads, RouteCraft keeps the largest strongly
connected component. In a directed graph, a strongly connected component is
a group of nodes where every node can reach every other node. Keeping the
largest one makes road-to-road routing between snapped locations more
reliable.

Kosaraju's algorithm makes two graph passes:

1. Traverse the forward graph and record nodes in finishing order.
2. Traverse the reversed graph in that order to collect components.

The largest component is retained, and edges leading outside it are removed.

## Time complexity at a glance

Let `C` be the number of customers, `V` the number of road nodes, and `E`
the number of road edges.

- Assigning customers to their nearest depot checks the depots for each
  customer: `O(C * D)`, where `D` is the number of depots.
- Clarke-Wright savings creates and sorts customer pairs: `O(C^2 log C)`,
  before route merges and route-score calculations.
- 2-opt examines pairs of stop positions: `O(S^2)` candidate pairs for a
  route of `S` stops.
- The bounded 3-opt pass examines triples of stop positions and evaluates
  candidate orderings, so it is the most expensive route-improvement step.
- Dijkstra with a binary heap and efficient decrease-key is
  `O((V + E) log V)`. The current Java implementation also removes an updated
  node from `PriorityQueue`, whose `remove(Object)` takes `O(V)`; route legs
  are cached to avoid repeating identical searches.
- Kosaraju's algorithm is `O(V + E)`.

## How to find the code

In [`RouteOptimizer.java`](./RouteOptimizer.java), the main methods are:

- `optimize(...)` — coordinates the route-planning steps.
- `nearestMultiDepot(...)` — nearest-neighbor construction heuristic.
- `savingsMultiDepot(...)` — Clarke-Wright savings heuristic.
- `twoOpt(...)` and `threeOptLite(...)` — improve the stop order.
- `shortestLeg(...)` — Dijkstra shortest-path search.
- `retainLargestStronglyConnectedComponent(...)` — Kosaraju graph traversal.

The interface and OpenStreetMap file-loading code remain in
[`RouteCraftVRP.java`](./RouteCraftVRP.java).
