# RouteCraft User Manual

RouteCraft creates capacity-constrained delivery routes from one or more depots.
It can use a simple coordinate map or an OpenStreetMap (OSM) road network.

## Start the application

On Windows, install a 64-bit JDK 17+ and a 64-bit C++17 compiler, set
`JAVA_HOME`, then double-click `run.bat`. To use saved OSM roads, download a
small `.osm` XML extract; `.pbf` files are not supported. Without an OSM file,
the application uses straight-line map distances.

## Plan and dispatch routes

1. Optionally choose **LOAD REAL MAP + OSM ROADS**, select an `.osm` file, and
   choose **FIT MAP** to frame the retained road network.
2. Add depots and customers with the side-panel controls, or choose **Place
   customer** / **Place depot** and click on the map. Set each customer's
   demand.
3. Set the vehicle count and capacity. A customer whose demand exceeds the
   capacity cannot be assigned.
4. Choose Clarke-Wright savings or nearest neighbor, then select
   **OPTIMIZE ROUTES**. Check the served-customer and fleet counts; the status
   reports if the fleet or capacity is insufficient.
5. Review route loads, distances, estimated times, and costs in the route
   manifest. Select **DISPATCH** to animate the vehicles. Use **PAUSE** or the
   speed and vehicle controls as needed.
6. Select **EXPORT CSV** to save the current route plan. Use undo/redo to
   restore recent scenario changes.

## Notes and troubleshooting

- Map tiles require internet access; routing on a loaded OSM graph works
  offline. Tile availability depends on the OpenStreetMap tile service.
- Distances are in kilometres only when an OSM road network is loaded.
  Coordinate-only routes have no real-world map scale and are reported in
  map units, not kilometres.
- Travel time and cost are estimates using static road speeds and the
  application's fixed model; without OSM roads they are synthetic estimates,
  not live-traffic estimates or currency quotes.
- If startup reports that the native library cannot be loaded, verify that
  `JAVA_HOME` points to a 64-bit JDK and that the C++ compiler has the same
  architecture, then run `run.bat` again.
- If OSM loading fails, use a small valid XML road extract and check that it
  contains routable highway ways.

For build instructions and tests, see the project `README.md`. For design,
implementation details, and references, see `RouteCraft_Report.tex`.
