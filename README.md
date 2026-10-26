# RouteCraft — Multi-Depot Vehicle Routing Studio

A Java Swing desktop app that solves the **Capacitated Multi-Depot Vehicle Routing Problem (MDVRP)** on real OpenStreetMap road networks. Import a map extract, place depots and customers, and compute routes that follow actual roads. Built as a DSA project.

## Features

- **Real road routing** — imports OpenStreetMap XML (`.osm`) and routes along the road graph using Dijkstra shortest paths.
- **Real map imagery** — OSM raster tiles are streamed for exactly the visible area; the tile zoom follows your zoom level.
- **Two solvers**
  - Clarke–Wright savings + 2-opt
  - Nearest neighbor + 2-opt
- **Capacitated, multi-depot** — set fleet size and per-vehicle capacity; stops are assigned across depots.
- **Weighted objective** — blend distance, travel time and operating cost with sliders.
- **Route manifest** — per-vehicle stop order, load, km, minutes, cost and score.
- **Live dispatch animation** — watch each vehicle drive its route (speed slider, pause, per-vehicle focus).
- **Scenario tools** — add, edit, duplicate and remove customers and depots; undo/redo; save/load scenarios; CSV import/export.

## Requirements

- JDK 17 or newer (`javac` on your PATH)
- Internet access, only for map tiles (routing works without it)

## Run

**Windows:** double-click `run.bat`, or in PowerShell:

```powershell
.\run.bat
```

**macOS / Linux:**

```bash
javac -encoding UTF-8 RouteCraftVRP.java RouteOptimizer.java
java RouteCraftVRP
```

## Quick start

1. Get a small `.osm` XML extract of your city area (for example, an OpenStreetMap export of a few km²). It must be **OSM XML**, not `.pbf`, a PDF or a screenshot.
2. Click **LOAD REAL MAP + OSM ROADS** and select the file. Map tiles load in the background.
3. Click **FIT MAP** to frame the road network.
4. Set the number of vehicles and the capacity per vehicle, then choose a solver.
5. Add customers and depots: set the mode to _Place customer_ or _Place depot_ and click the map, or use the side-panel forms.
6. Click **OPTIMIZE ROUTES**, then **DISPATCH** to animate the fleet.

## Map controls

| Action            | Control                               |
| ----------------- | ------------------------------------- |
| Zoom              | Mouse wheel (zooms around the cursor) |
| Pan               | Right-click or middle-click and drag  |
| Reset view        | **FIT MAP**                           |
| Inspect a node    | Hover over it or click it             |
| Focus one vehicle | "All vehicles" drop-down              |

## How it works

- **Projection:** tiles, road overlay and markers share one Web Mercator transform, so they stay aligned at every zoom level.
- **Road graph:** highways usable by motor vehicles are read from the OSM file, with one-way, `maxspeed` and toll tags respected. Only the **largest strongly connected component** is kept, so every stop can reach every other. Disconnected fragments (service stubs, clipped highway ends) are discarded.
- **Snapping:** customers and depots snap to the nearest node in that connected network. The 0–100 X/Y values in the side panel are relative positions within the road network's bounding box.
- **Costs:** each edge has distance, travel time (OSM speed tag or road-class default) and cost (₹12 per km, plus a toll surcharge). The objective score is the weighted sum set by the sliders.
- **Solvers:** `RouteOptimizer.java` contains the route data models and DSA algorithms: Clarke–Wright savings, nearest neighbor, 2-opt, a light 3-opt pass, weighted Dijkstra shortest paths, and Kosaraju's strongly connected component filter. `RouteCraftVRP.java` contains the Swing interface, OSM import and map rendering.

## Notes and limitations

- The solvers are heuristics and do not guarantee a globally optimal solution.
- Travel time uses static speeds. No live traffic is included.
- Use compact extracts: very large files are slow to parse because the whole XML is loaded into memory.
- Tiles come from the public OpenStreetMap tile server. Please follow the [tile usage policy](https://operations.osmfoundation.org/policies/tiles/) and avoid heavy use.
- If tiles show as unavailable, the roads and routes still work; check your network or firewall.

## Project layout

```
RouteCraft_Ultimate/
├── RouteCraftVRP.java   # Swing interface, OSM import and map rendering
├── RouteOptimizer.java  # DSA models and routing algorithms
├── DSA_EXPLANATION.md   # DSA overview and presentation guide
├── run.bat              # Windows build-and-run script
└── README.md
```

Both Java source files are compiled together by `run.bat`. The optimizer is
kept separate from the interface so its algorithms can be read and explained
independently.

## Credits

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright).
