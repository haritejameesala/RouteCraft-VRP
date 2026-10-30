# RouteCraft – User Manual

## 1. Start the Application
- Install JDK 17 or later and a C++17 compiler.
- Set the `JAVA_HOME` environment variable.
- Run `run.bat` to start the application.

## 2. Add Locations
- Add depots and customers using the side panel.
- Enter each customer's demand.
- Optionally, load an `.osm` file to use real road networks.

## 3. Set Vehicle Details
- Enter the number of vehicles and their capacity.
- Customers whose demand exceeds vehicle capacity cannot be served.

## 4. Optimize Routes
- Select **Clarke-Wright Savings** or **Nearest Neighbor**.
- Click **OPTIMIZE ROUTES**.
- Review the routes, vehicle loads, distances, and customers served.

## 5. Dispatch Vehicles
- Click **DISPATCH** to animate the vehicles.
- Use **PAUSE** and the speed controls to manage the simulation.

## 6. Export Routes
- Click **EXPORT CSV** to save the route plan.
- Use Undo and Redo to reverse or restore recent changes.

## 7. Important Notes
- Internet access is required to load map tiles.
- Without an OSM road network, distances are shown in map units, not kilometres.
- Travel times and costs are estimates, not live traffic data or actual prices.
- If the application fails to start or load a map, check the installation requirements and OSM file format.

For more details, see `README.md`.