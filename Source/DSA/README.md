# RouteCraft DSA Guide

This folder contains the algorithm implementation and native tests. Java and JNI
integration are kept separately in `Source/Java/` and `Source/JNI/`.

## What problem does the DSA code solve?

Given customer demands, depots, vehicle capacities, and an optional road graph,
the engine creates routes that serve customers without exceeding vehicle
capacity. It tries to keep routes efficient, improve their visit order, and report
when the configured fleet cannot serve everyone.

## Follow the algorithm flow

1. **Represent the data** — `RouteObjects.cpp` implements the shared node, road,
   route, and travel-leg models.
2. **Choose a depot and build routes** — `RouteConstructionAlgorithms.cpp`
   assigns customers to depots, then builds routes using either Nearest Neighbor
   or Clarke-Wright Savings. Capacity is checked as customers are added.
3. **Fit the fleet** — Savings construction also considers legal route merges
   when it needs to reduce the number of routes. Elastic-fleet allocation is
   represented in the route results.
4. **Measure and improve** — `RouteImprovementAlgorithms.cpp` calculates route
   distance, time, cost, and weighted score, validates inputs, and applies 2-opt
   to improve the order of stops.
5. **Use real roads when available** — `RoadNetworkAlgorithms.cpp` finds
   shortest paths with Dijkstra and filters the road graph to its largest
   strongly connected component using Kosaraju's algorithm. Without a loaded
   road graph, geographic distance is used.
6. **Check the behavior** — `RouteCraftDsaTests.cpp` tests route metrics,
   capacity and customer coverage, fleet tiers, graph connectivity, and input
   validation.

The shared declarations for these files are in `include/DSA/`. The DSA engine
does not depend on Swing or JNI; the application reaches it through the separate
bridge and adapter.

## Short explanation for a project review

> “Our DSA engine models depots, customers, vehicles, and roads as structured
> data. It first assigns customers to depots and builds capacity-limited routes
> using Nearest Neighbor or Clarke-Wright Savings. It then measures each route
> and applies 2-opt to reduce unnecessary travel. When road data is loaded, it
> uses Dijkstra for shortest paths and Kosaraju to select the largest strongly
> connected road component. Native tests verify route feasibility, metrics, and
> graph behavior. Java and JNI only connect this engine to the user interface.”

## File map

| File | Main responsibility |
|---|---|
| `RouteObjects.cpp` | Shared route, node, road, and leg model methods |
| `RouteConstructionAlgorithms.cpp` | Depot assignment, Nearest Neighbor, Clarke-Wright Savings, and route merges |
| `RouteImprovementAlgorithms.cpp` | Validation, route metrics, scoring, elastic-fleet planning, and 2-opt |
| `RoadNetworkAlgorithms.cpp` | Dijkstra shortest paths and Kosaraju strongly connected components |
| `RouteCraftDsaTests.cpp` | Standalone native tests for the DSA engine |

Build and run the native tests from the project root with `mingw32-make a.out`
followed by `.\a.out`.
