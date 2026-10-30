# RouteCraft — Multi-Depot Vehicle Routing Studio

**A Data Structures and Algorithms project by Team AlgoSmiths**

RouteCraft is a desktop-based vehicle routing and fleet optimization application built using **Java Swing, C++17, and Java Native Interface (JNI)**. It addresses the Capacitated Multi-Depot Vehicle Routing Problem (MDVRP) by assigning customer deliveries to vehicles operating from multiple depots while considering vehicle capacity, route distance, estimated travel time, and transportation cost.

The application supports OpenStreetMap (OSM) road networks, heuristic route construction, shortest-path computation, local route optimization, route visualization, dispatch animation, and CSV export.

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Project Objectives](#2-project-objectives)
3. [Key Features](#3-key-features)
4. [Technology Stack](#4-technology-stack)
5. [System Architecture](#5-system-architecture)
6. [Data Structures and Algorithms](#6-data-structures-and-algorithms)
7. [Route Optimization Workflow](#7-route-optimization-workflow)
8. [Project Structure](#8-project-structure)
9. [Prerequisites](#9-prerequisites)
10. [Installation and Setup](#10-installation-and-setup)
11. [Running the Application](#11-running-the-application)
12. [Using RouteCraft](#12-using-routecraft)
13. [Testing](#13-testing)
14. [Input Data and Routing Metrics](#14-input-data-and-routing-metrics)
15. [Limitations](#15-limitations)
16. [Future Enhancements](#16-future-enhancements)
17. [Documentation](#17-documentation)
18. [Credits and Data Attribution](#18-credits-and-data-attribution)

---

## 1. Problem Statement

Vehicle routing is a fundamental optimization problem in logistics, transportation, and supply chain management. Organizations need to deliver goods from warehouses or distribution centers to multiple customers while minimizing transportation costs and satisfying operational constraints.

The problem becomes more challenging when:

- Multiple depots are available.
- Each vehicle has a limited carrying capacity.
- Customers have different delivery demands.
- Roads have different distances, speed limits, and transportation costs.
- Road networks contain one-way streets and disconnected components.
- The fleet size is limited.
- Route quality must be balanced across multiple optimization criteria.

Finding the globally optimal solution for general vehicle routing problems is computationally expensive. RouteCraft uses a combination of graph algorithms, greedy heuristics, and local search techniques to generate practical delivery routes.

### Proposed solution

RouteCraft models the road network as a weighted graph and uses C++ algorithms to construct and improve vehicle routes. A Java Swing interface allows users to load map data, configure the fleet, add depots and customers, optimize routes, inspect route metrics, visualize the delivery plan, and export results.

The project demonstrates how fundamental data structures and algorithms can be combined to solve a real-world optimization problem.

---

## 2. Project Objectives

The primary objectives of RouteCraft are:

1. **Efficient route planning:** Generate practical delivery routes for multiple vehicles and depots.
2. **Capacity-aware allocation:** Respect vehicle capacity constraints while assigning customer deliveries.
3. **Graph-based navigation:** Represent real road networks as graphs and calculate shortest paths.
4. **Multi-criteria optimization:** Support route evaluation using distance, time, and cost.
5. **Route improvement:** Reduce unnecessary travel through local search techniques.
6. **Interactive visualization:** Display the road network, customer locations, depots, and optimized routes.
7. **Native algorithm integration:** Implement the computationally intensive routing engine in C++ and connect it to Java through JNI.
8. **Result export:** Allow users to save route plans for further analysis.

---

## 3. Key Features

### 3.1 OpenStreetMap integration

- Load road networks from OpenStreetMap XML (`.osm`) files.
- Process road nodes, road connections, one-way restrictions, and available road attributes.
- Use road-network connectivity for route computation.
- Display map tiles when an internet connection is available.
- Fit the map view to the loaded network.

### 3.2 Multi-depot vehicle routing

- Create multiple depots.
- Add customers with individual delivery demands.
- Configure the available vehicle count.
- Set vehicle carrying capacity.
- Generate routes using the selected heuristic.
- Identify situations where fleet capacity is insufficient.

### 3.3 Route optimization

- Clarke–Wright savings heuristic.
- Nearest-neighbor route construction.
- 2-opt local search.
- 3-opt-lite local search.
- Weighted shortest-path computation over road graphs.

### 3.4 Multiple routing metrics

Routes can be evaluated using:

- Distance.
- Estimated travel time.
- Estimated transportation cost.
- A weighted routing score.

### 3.5 Interactive desktop interface

- Java Swing graphical user interface.
- Map-based customer and depot placement.
- Route visualization.
- Route manifest with delivery and route information.
- Vehicle dispatch animation.
- Dispatch pause and speed controls.
- Scenario editing with undo/redo support.
- CSV export of the current route plan.

### 3.6 Modular implementation

The application separates the user interface from the algorithm engine. Java handles the desktop interface and OSM input processing, while C++ performs the routing and graph computations through a native interface.

---

## 4. Technology Stack

| Technology | Purpose |
|---|---|
| Java | Application logic and desktop interface |
| Java Swing | Graphical user interface |
| C++17 | Routing engine and DSA implementation |
| JNI | Communication between Java and C++ |
| OpenStreetMap | Geographic road-network data |
| XML | OSM map input format |
| Make | Build automation and test execution |
| GCC / MinGW-w64 | C++ compilation |
| JDK 17+ | Java compilation and execution |
| CSV | Exporting route plans |

### Why Java and C++?

Java Swing provides a convenient framework for an interactive desktop application. C++ provides direct control over graph structures and efficient implementation of routing algorithms.

JNI connects the two components without requiring the routing algorithms to be reimplemented in Java.

---

## 5. System Architecture

RouteCraft follows a layered architecture consisting of the presentation layer, Java integration layer, native C++ engine, and road-network model.

### Architecture overview

```text
+--------------------------------------------------+
|              Java Swing Interface                |
|                                                  |
|  Map View | Fleet Configuration | Route Manifest |
|  Dispatch | Scenario Editing | CSV Export        |
+------------------------+-------------------------+
                         |
                         v
+--------------------------------------------------+
|             Java Integration Layer               |
|                                                  |
|  RouteOptimizerBridge.java                       |
|  Java Data Models and JNI Calls                  |
|  OSM XML Processing and Graph Transfer           |
+------------------------+-------------------------+
                         |
                         v
+--------------------------------------------------+
|                 JNI Interface                    |
|                                                  |
|  Java-to-C++ Calls and Result Conversion         |
+------------------------+-------------------------+
                         |
                         v
+--------------------------------------------------+
|                C++ Routing Engine                |
|                                                  |
|  Graph Structures                                |
|  Dijkstra's Shortest Path                        |
|  Kosaraju's SCC Algorithm                        |
|  Clarke-Wright Savings                           |
|  Nearest Neighbor                                |
|  2-opt and 3-opt-lite                            |
|  Route Metrics and Geographic Distance           |
+------------------------+-------------------------+
                         |
                         v
+--------------------------------------------------+
|               Optimized Route Plan               |
|                                                  |
|  Vehicle Routes | Customer Assignments           |
|  Distance | Estimated Time | Estimated Cost      |
+--------------------------------------------------+
```

### Component responsibilities

**Java Swing application — `RouteCraftVRP.java`**

Provides the desktop interface, user interaction, map display, scenario management, route presentation, dispatch animation, and CSV export.

**Java native bridge — `RouteOptimizerBridge.java`**

Transfers graph and routing data between Java and C++. It exposes native functionality to the interface and converts native results into Java-side models.

**Route construction — `RouteConstructionAlgorithms.cpp`**

Assigns customers to depots and builds capacity-aware routes using nearest neighbor or Clarke–Wright savings. It also merges routes to fit the fleet.

**Route improvement and metrics — `RouteImprovementAlgorithms.cpp`**

Validates optimization inputs, scores routes, calculates distance/time/cost metrics, and improves stop order with 2-opt and 3-opt-lite.

**C++ data models — `RouteObjects.cpp`**

Implements the native model methods used by the routing engine.

**Road-network algorithms — `RoadNetworkAlgorithms.cpp`**

Implements nearest road-node lookup, weighted Dijkstra shortest paths, largest strongly connected component filtering, and geographic distance calculation.

**JNI adapter — `integration/jni/RouteOptimizerJni.cpp`**

Exposes C++ operations to Java through native method bindings.

**Header files — `include/`**

Define the interfaces and data structures shared by the C++ implementation.

---

## 6. Data Structures and Algorithms

The project uses several data structures and algorithms to represent road networks, calculate shortest paths, construct vehicle routes, and improve route quality.

### 6.1 Graph representation

The road network is represented as a weighted directed graph:

\[
G=(V,E)
\]

where:

- \(V\) is the set of road intersections or OSM nodes.
- \(E\) is the set of directed road connections.
- Each edge stores routing attributes such as distance, estimated time, and cost.

A directed graph is necessary because some roads permit travel in only one direction.

The graph representation supports shortest-path search and connectivity analysis.

### 6.2 Dijkstra's shortest-path algorithm

Dijkstra's algorithm computes shortest paths from a source node when edge weights are non-negative.

For an edge \((u,v)\), the relaxation operation is:

\[
d[v]=\min(d[v],d[u]+w(u,v))
\]

where \(w(u,v)\) is the selected edge weight.

RouteCraft uses weighted shortest-path computation to obtain road-network paths between locations. The selected routing metric influences the edge weights.

**Typical complexity:** \(O((|V|+|E|)\log |V|)\) with an adjacency-list representation and a binary heap.

### 6.3 Kosaraju's strongly connected components

Kosaraju's algorithm identifies strongly connected components in a directed graph using graph traversal and a reversed graph.

It performs two DFS-based passes:

1. Determine vertices in finishing-time order.
2. Traverse the reversed graph in that order to identify components.

The implementation uses strongly connected component processing to retain the largest component of the imported road graph. This improves consistency for routing within the retained network, but may discard roads outside that component.

**Complexity:** \(O(|V|+|E|)\).

### 6.4 Clarke–Wright savings heuristic

The Clarke–Wright savings algorithm starts with separate depot-to-customer routes and considers merging routes when the merge improves the solution.

For customers \(i\) and \(j\), the classical savings value is:

\[
S(i,j)=d(0,i)+d(0,j)-d(i,j)
\]

where \(0\) denotes the depot and \(d(a,b)\) denotes the relevant travel metric.

The algorithm considers savings and combines compatible routes while respecting capacity and route constraints.

**Purpose:** Construct efficient routes by reducing redundant depot travel.

The savings heuristic is a practical approximation and does not guarantee the globally optimal MDVRP solution.

### 6.5 Nearest-neighbor heuristic

The nearest-neighbor heuristic repeatedly selects the nearest eligible unvisited customer.

At each step, it chooses the next feasible customer based on the selected distance or routing score, subject to the implementation's route and capacity rules.

**Advantages:**

- Simple to understand.
- Fast to execute.
- Provides a practical baseline for route construction.

**Limitation:** A locally nearest choice may lead to a poor overall route.

### 6.6 2-opt local search

2-opt improves a route by removing two edges and reconnecting the route in an alternative way.

For a route containing edges \((a,b)\) and \((c,d)\), a candidate change replaces them with \((a,c)\) and \((b,d)\), subject to route feasibility and the implementation's evaluation criteria.

The process searches for improvements until no accepted 2-opt move remains or the implementation's stopping conditions are reached.

### 6.7 3-opt-lite local search

3-opt considers removing three edges and reconnecting the resulting route segments in different configurations.

RouteCraft uses a 3-opt-lite improvement stage to explore additional route modifications beyond 2-opt. This can improve route quality but generally requires more computation.

### 6.8 Geographic-distance calculations

The native engine also provides geographic-distance calculations based on latitude and longitude. These calculations support location comparisons and routing operations when geographic coordinates are available.

### 6.9 Memory management

The C++ implementation uses smart pointers for graph and route objects.

- `std::shared_ptr` manages shared ownership of graph nodes and routes.
- `std::weak_ptr` represents non-owning neighbor references.

This helps avoid ownership cycles in the road graph.

### Algorithm summary

| Algorithm | Primary purpose |
|---|---|
| Dijkstra | Shortest paths on a weighted road graph |
| Kosaraju | Strongly connected component identification |
| Clarke–Wright savings | Construct efficient vehicle routes |
| Nearest neighbor | Greedy route construction |
| 2-opt | Improve route ordering |
| 3-opt-lite | Explore further route improvements |
| Geographic-distance calculation | Estimate geographic separation |

---

## 7. Route Optimization Workflow

The application follows this general workflow:

1. Load an OSM road network or use the coordinate-based map.
2. Parse road nodes, connections, and available road attributes.
3. Transfer the road graph to the C++ engine.
4. Process graph connectivity and retain the largest strongly connected component.
5. Add depots and customer locations.
6. Configure the number of vehicles and vehicle capacity.
7. Select the Clarke–Wright savings or nearest-neighbor method.
8. Construct feasible candidate routes.
9. Improve route ordering using local search.
10. Calculate route metrics.
11. Display the resulting plan in the Java interface.
12. Animate dispatch or export the route plan to CSV.

The exact route produced depends on the input graph, customer demands, fleet configuration, heuristic, and selected routing weights.

### Capacity constraint

For a vehicle with capacity \(Q\), a route must satisfy:

\[
\sum_{i\in R} q_i\le Q
\]

where \(q_i\) is the demand of customer \(i\), and \(R\) is the set of customers assigned to that vehicle.

If the available fleet cannot accommodate all customer demands, the application may report an insufficient fleet or capacity condition rather than serving every customer.

---

## 8. Project Structure

```text
RouteCraft/
│
├── README.md
├── Makefile
├── run.bat
├── .gitignore
├── a.out
│
├── RouteCraftVRP.java
├── RouteOptimizerBridge.java
├── RouteOptimizerBridgeTest.java
│
├── include/                         # DSA declarations only
│   ├── RouteObjects.h
│   ├── RouteOptimizer.h
│   └── RoadNetworkAlgorithms.h
│
├── source/                          # DSA implementation and native tests only
│   ├── RouteObjects.cpp
│   ├── RouteConstructionAlgorithms.cpp
│   ├── RouteImprovementAlgorithms.cpp
│   ├── RoadNetworkAlgorithms.cpp
│   └── RouteCraftDsaTests.cpp
│
├── integration/
│   └── jni/
│       ├── RouteOptimizerJni.h
│       └── RouteOptimizerJni.cpp
│
├── object/
│   ├── RouteCraftDsaTests.o
│   ├── RouteObjects.o
│   ├── RouteConstructionAlgorithms.o
│   ├── RouteImprovementAlgorithms.o
│   ├── RoadNetworkAlgorithms.o
│   └── RouteOptimizerJni.o
│
├── build/
│   ├── RouteCraftDsa.dll
│   ├── Java compiled classes
│   └── Java build stamp
│
├── Document/
│   ├── Assignment_Guidelines.pdf
│   ├── User_Manual.md
│   ├── RouteCraft_Flowchart.tex
│   └── RouteCraft_Report.tex
│
└── Video/
    └── Project_Video_Instructions.md
```

### Important files

| File | Description |
|---|---|
| `RouteCraftVRP.java` | Java Swing application |
| `RouteOptimizerBridge.java` | Java-to-C++ integration layer |
| `RouteOptimizerBridgeTest.java` | Java/JNI integration tests |
| `RouteObjects.h` | C++ model declarations |
| `RouteObjects.cpp` | C++ model implementations |
| `RouteOptimizer.h` | Shared route-optimizer declarations |
| `RouteConstructionAlgorithms.cpp` | Depot assignment, nearest-neighbor and savings construction, capacity, and fleet merges |
| `RouteImprovementAlgorithms.cpp` | Objective scoring, route metrics, input validation, 2-opt, and 3-opt-lite |
| `RoadNetworkAlgorithms.h` | Road-graph algorithm declarations |
| `RoadNetworkAlgorithms.cpp` | Dijkstra, strongly connected components, and geographic distance |
| `integration/jni/RouteOptimizerJni.h` | JNI interface declarations |
| `integration/jni/RouteOptimizerJni.cpp` | Java-to-native integration methods |
| `RouteCraftDsaTests.cpp` | Native algorithm tests |
| `Makefile` | Build and test automation |
| `run.bat` | Windows build and launch script |
| `Document/User_Manual.md` | User instructions and troubleshooting |
| `Document/RouteCraft_Report.tex` | Project report source |
| `Document/RouteCraft_Flowchart.tex` | Flowchart source |
| `Video/Project_Video_Instructions.md` | Video preparation instructions |

The compiled artifacts in `object/`, `build/`, and `a.out` are generated files. They may be recreated from the source code using the appropriate build commands.

### DSA split for three presenters

The three algorithm modules are similar in implementation size (about 230–250 lines each) and can be presented as three member assignments:

1. **Member 1 — Route construction:** `RouteConstructionAlgorithms.cpp` covers nearest-depot assignment, capacity-aware nearest-neighbor routing, Clarke–Wright savings, endpoint-compatible merges, and reducing routes to the fleet limit.
2. **Member 2 — Route improvement and evaluation:** `RouteImprovementAlgorithms.cpp` covers input checks, the weighted objective, round-trip metrics, and 2-opt/3-opt-lite route improvements.
3. **Member 3 — Road-network algorithms:** `RoadNetworkAlgorithms.cpp` covers nearest-road-node search, Dijkstra shortest paths, Kosaraju strongly connected components, and haversine distance.

`RouteObjects.h`/`RouteObjects.cpp` provide shared data structures used by all three parts. `RouteOptimizer.h` provides their shared interface. The JNI adapter is integration code, not a DSA module, so it remains under `integration/jni/`.

---

## 9. Prerequisites

### Windows

Install the following:

1. **JDK 17 or newer**, including `javac`.
2. **64-bit C++17 compiler**, preferably MinGW-w64 from MSYS2 UCRT64.
3. **GNU Make**, available as `mingw32-make` if using the supplied Makefile.
4. Git, if cloning the repository.

Configure `JAVA_HOME` to point to the JDK installation directory.

For example, if the JDK is installed at `C:\Program Files\Java\jdk-21`, configure:

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
$env:Path = "$env:JAVA_HOME\bin;$env:Path"
```

Use the actual installation path on your system.

Verify the installations:

```powershell
java -version
javac -version
g++ --version
```

### macOS and Linux

Install:

- JDK 17 or newer.
- A C++17-compatible compiler.
- A compatible build tool such as GNU Make.
- Git, if required.

Set `JAVA_HOME` to the active JDK installation.

The supplied Windows launcher and Makefile use Windows-specific JNI library settings. On macOS and Linux, compile the native library with the appropriate JNI include directories and platform-specific shared-library name.

---

## 10. Installation and Setup

### Option A: Obtain the project source

If the project is available as a Git repository:

```bash
git clone <repository-url>
cd RouteCraft
```

Replace `<repository-url>` with the actual repository URL.

If working from the submitted ZIP, extract it and open a terminal in the directory containing `README.md`, `Makefile`, and `run.bat`.

### Option B: Run on Windows

The simplest supported launch method is:

```powershell
.\run.bat
```

The script:

1. Checks for Java compilation tools.
2. Locates a suitable C++ compiler.
3. Checks the compiler architecture.
4. Compiles the C++ routing engine and JNI adapter into `build\RouteCraftDsa.dll`.
5. Compiles the Java application into `build/`.
6. Launches the application with the native library path configured.

If the build fails, review the compiler output and verify `JAVA_HOME`, the JDK architecture, and the compiler installation.

### Option C: Build and test with Make

From a Windows command prompt with the recommended MSYS2 toolchain and `mingw32-make` available:

```powershell
mingw32-make all
```

To execute the native and Java integration tests:

```powershell
mingw32-make test
```

To remove generated build artifacts:

```powershell
mingw32-make clean
```

The Makefile is configured for the Windows JNI environment. For other operating systems, use equivalent compiler commands with the correct platform-specific JNI include directories and output library name.

---

## 11. Running the Application

On Windows, run:

```powershell
.\run.bat
```

The application should open after the native library and Java classes compile successfully.

Alternatively, after building the application:

```powershell
java -Djava.library.path=build -cp build RouteCraftVRP
```

The native library must be available in the `build` directory, and the compiled Java classes must be present on the classpath.

On Windows, the expected native library is `RouteCraftDsa.dll`. The launcher configures the native library path automatically.

---

## 12. Using RouteCraft

### Step 1: Load a road network

Select **LOAD REAL MAP + OSM ROADS** and choose a valid `.osm` XML file.

Use a reasonably small extract for initial testing. The application can also operate without an imported road graph, using coordinate-based routing.

### Step 2: Fit the map

Select **FIT MAP** to frame the displayed network.

Map tiles require internet access. The routing engine can operate on an already loaded road graph without continuously accessing the internet.

### Step 3: Add depots and customers

Create one or more depots and add customers.

Customers can be placed using the side-panel controls or the map placement modes. Set the demand for each customer.

### Step 4: Configure the fleet

Set:

- Number of available vehicles.
- Vehicle capacity.
- Route construction method.

Choose either Clarke–Wright savings or nearest neighbor.

### Step 5: Optimize routes

Select **OPTIMIZE ROUTES**.

Inspect the generated routes, customer assignments, route loads, and available route metrics. Pay attention to any indication that the fleet or capacity is insufficient.

### Step 6: Review the route manifest

Review the route manifest to understand the planned deliveries and the calculated route metrics.

Depending on the loaded data and configuration, these include distance, estimated time, cost, and route score.

### Step 7: Dispatch vehicles

Select **DISPATCH** to animate the vehicles along their planned routes.

Use the available pause, speed, and vehicle controls to inspect the dispatch process.

### Step 8: Export the plan

Select **EXPORT CSV** to save the current route plan for further inspection.

Scenario changes can also be managed through the available undo/redo controls.

For detailed interface instructions, refer to `Document/User_Manual.md`.

---

## 13. Testing

RouteCraft includes native C++ tests and Java/JNI integration tests.

### 13.1 Native C++ tests

The native test source is:

```text
source/RouteCraftDsaTests.cpp
```

The Makefile builds these tests into the `a.out` executable.

Run the native test executable from the project directory:

```bash
./a.out
```

On Windows, the executable may be named `a.out.exe`, depending on the compiler and build environment.

### 13.2 Java/JNI integration tests

The integration test source is:

```text
RouteOptimizerBridgeTest.java
```

After compiling the application and native library, execute:

```bash
java -Djava.library.path=build -cp build RouteOptimizerBridgeTest
```

The integration tests exercise native graph transfer, shortest-path behavior, route metrics, and optimization through the Java bridge.

### 13.3 Run the complete test suite

On the configured Windows build environment:

```powershell
mingw32-make test
```

This target builds the project and runs both the native tests and the Java integration tests.

### 13.4 Recommended manual validation

In addition to automated tests, verify the following through the GUI:

- Load an OSM extract and fit the map.
- Create multiple depots and customers.
- Change vehicle count and capacity.
- Compare Clarke–Wright savings with nearest neighbor.
- Inspect route loads and route metrics.
- Test insufficient fleet capacity.
- Test one-way roads and disconnected road networks.
- Exercise dispatch, pause, and speed controls.
- Export a route plan to CSV.
- Verify undo/redo behavior.

Automated tests cover only the scenarios explicitly implemented in the test suite. Manual validation is necessary to assess the complete graphical workflow.

---

## 14. Input Data and Routing Metrics

### OpenStreetMap input

RouteCraft accepts OSM XML extracts with the `.osm` extension. It does not support `.pbf` files directly.

The quality of the generated routes depends on the completeness of the road network, the connectivity of the graph, the locations of depots and customers, and the road attributes available in the input.

### Distance

When an OSM road graph is loaded, route distances can be expressed in kilometres using the road-edge distances.

Without OSM roads, the application uses coordinate-based map distances. These coordinates do not necessarily have a real-world scale, so the resulting values are reported in map units rather than kilometres.

### Estimated travel time

Travel-time estimates use available road-speed information and the application's static model. They are estimates, not live traffic predictions.

### Estimated transportation cost

Cost calculations depend on the application's routing model and available edge attributes. They should not be interpreted as live fuel prices, market prices, or actual transportation quotations.

### Weighted route score

The routing engine supports weighted route evaluation. Conceptually, a score can combine normalized distance, time, and cost:

\[
J(R)=w_dD(R)+w_tT(R)+w_cC(R)
\]

where:

- \(R\) is a route.
- \(D(R)\) is its distance.
- \(T(R)\) is its estimated travel time.
- \(C(R)\) is its estimated cost.
- \(w_d,w_t,w_c\) are the corresponding weights.

The precise score calculation and default weights are determined by the implementation. The expression above illustrates the general weighted-objective concept; it does not imply that every configuration uses this exact normalized formula.

---

## 15. Limitations

1. **Heuristic optimization:** Clarke–Wright, nearest neighbor, 2-opt, and 3-opt-lite do not guarantee a globally optimal solution.
2. **Static travel times:** Live traffic, road closures, and real-time travel delays are not incorporated into the static routing model.
3. **OSM graph connectivity:** The implementation retains the largest strongly connected component. Some roads and locations outside it may be discarded.
4. **Input format:** The documented map-loading workflow uses OSM XML rather than PBF files.
5. **Fleet feasibility:** Limited vehicle count or capacity can prevent all customer demands from being served.
6. **Cost estimation:** Estimated costs depend on the model and input attributes, not live commercial transportation data.
7. **Map tiles:** Online map display depends on network availability and the OpenStreetMap tile service.
8. **Platform-specific builds:** The supplied Windows launcher and Makefile use Windows-specific JNI paths. Other operating systems require platform-appropriate build configuration.
9. **Scalability:** Performance depends on road-graph size, customer count, and the number of route-improvement operations.

These limitations define the current scope of the project and provide opportunities for further development.

---

## 16. Future Enhancements

Potential extensions include:

- Real-time traffic integration.
- Time-window-constrained deliveries.
- Vehicle-specific capacities and operating costs.
- Electric vehicle routing with charging-station constraints.
- Dynamic customer requests and route replanning.
- Better handling of disconnected and partially connected road networks.
- More advanced metaheuristics such as Tabu Search, Simulated Annealing, or Genetic Algorithms.
- Benchmarking against known vehicle-routing instances.
- Detailed performance comparisons between route construction and improvement algorithms.
- Improved route analytics and visual reporting.
- More extensive automated tests for infeasible and large-scale instances.

These are proposed future improvements and are not all implemented in the current version.

---

## 17. Documentation

The project includes the following supporting documents:

| Document | Location |
|---|---|
| Project report source | `Document/RouteCraft_Report.tex` |
| Flowchart source | `Document/RouteCraft_Flowchart.tex` |
| User manual | `Document/User_Manual.md` |
| Assignment guidelines | `Document/Assignment_Guidelines.pdf` |
| Video instructions | `Video/Project_Video_Instructions.md` |

The report and flowchart are provided as LaTeX source files. Compile them to obtain the corresponding PDF documents.

---

## 18. Credits and Data Attribution

### OpenStreetMap

RouteCraft uses road-network data from OpenStreetMap.

Map data © OpenStreetMap contributors.

- Website: https://www.openstreetmap.org/
- Copyright and attribution: https://www.openstreetmap.org/copyright
- Tile usage policy: https://operations.osmfoundation.org/policies/tiles/

Users should comply with the applicable OpenStreetMap attribution requirements and tile usage policy when using or distributing the application.

### Project team

**Team:** AlgoSmiths

This project demonstrates the application of graph algorithms, greedy heuristics, local search, native programming interfaces, and desktop application development to a practical logistics optimization problem.

---

**RouteCraft — Optimizing routes through data structures and algorithms.**
