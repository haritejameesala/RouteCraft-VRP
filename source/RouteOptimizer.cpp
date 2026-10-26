#include "RouteOptimizer.h"
#include <cmath>
#include <queue>
#include <algorithm>
#include <stack>
#include <limits>
#include <stdexcept>
#include <utility>
constexpr double TWO_OPT_EPSILON = 1e-9;
constexpr double THREE_OPT_EPSILON = 1e-8;
RouteOptimizer::RouteOptimizer(std::unordered_map<long long, std::shared_ptr<RoadNode>>& roadNodes) : roadNodes(roadNodes) {}
void RouteOptimizer::setWeights(double d, double t, double c) { weightDistance = d; weightTime = t; weightCost = c; clearCache(); }
void RouteOptimizer::clearCache() { legCache.clear(); }
void RouteOptimizer::setRoadNetworkLoaded(bool l) { roadNetworkLoaded = l; clearCache(); }
double RouteOptimizer::distance(const std::shared_ptr<Node>& f, const std::shared_ptr<Node>& t) { return leg(f, t).score; }
double RouteOptimizer::objectiveScore(double km, double minutes, double cost) const { return weightDistance * km + weightTime * (minutes / 60.0 * 30.0) + weightCost * (cost / 12.0); }
std::vector<Route> RouteOptimizer::optimize(const std::vector<std::shared_ptr<Node>>& c, const std::vector<std::shared_ptr<Node>>& d, int v, int cap, bool s) {
    if (v <= 0) throw std::invalid_argument("Vehicle count must be greater than zero.");
    if (cap <= 0) throw std::invalid_argument("Vehicle capacity must be greater than zero.");
    if (!c.empty() && d.empty()) throw std::invalid_argument("At least one depot is required to route customers.");
    for (const auto& customer : c) {
        if (!customer || customer->demand < 0) throw std::invalid_argument("Customers must have non-negative demand.");
    }
    for (const auto& depot : d) {
        if (!depot) throw std::invalid_argument("Depot entries must not be null.");
    }
    if (c.empty()) return {};
    this->vehicleCount = v; this->capacity = cap; clearCache();
    std::vector<Route> routes = s ? savingsMultiDepot(c, d) : nearestMultiDepot(c, d);
    for (auto& r : routes) { twoOpt(r); threeOptLite(r); }
    routes.erase(std::remove_if(routes.begin(), routes.end(), [](const Route& r) { return r.stops.empty(); }), routes.end());
    return routes;
}
Leg RouteOptimizer::leg(const std::shared_ptr<Node>& from, const std::shared_ptr<Node>& to) {
    std::string key = std::to_string(from->id) + ":" + (from->isDepot ? "D" : "C") + ":" + std::to_string(to->id) + ":" + (to->isDepot ? "D" : "C");
    if (legCache.find(key) != legCache.end()) return legCache[key];
    Leg result;
    if (roadNetworkLoaded && !roadNodes.empty()) {
        auto start = nearestRoadNode(from->x, from->y); auto end = nearestRoadNode(to->x, to->y);
        if (start && end) { result = shortestLeg(start, end); if (!result.path.empty()) { legCache[key] = result; return result; } }
    }
    double sl = std::hypot(from->x - to->x, from->y - to->y);
    result.km = sl; result.minutes = sl * 2.0; result.cost = sl * 12.0;
    result.score = objectiveScore(result.km, result.minutes, result.cost);
    legCache[key] = result; return result;
}
std::shared_ptr<RoadNode> RouteOptimizer::nearestRoadNode(double x, double y) {
    std::shared_ptr<RoadNode> nearest = nullptr; double best = std::numeric_limits<double>::max();
    for (const auto& entry : roadNodes) { const auto& n = entry.second; double d = std::hypot(n->x - x, n->y - y); if (d < best) { best = d; nearest = n; } }
    return nearest;
}
std::unordered_map<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>> RouteOptimizer::assignCustomersToDepots(const std::vector<std::shared_ptr<Node>>& c, const std::vector<std::shared_ptr<Node>>& d) {
    std::unordered_map<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>> g;
    for (const auto& dep : d) g[dep] = std::vector<std::shared_ptr<Node>>();
    for (const auto& cust : c) {
        auto cl = d[0];
        for (const auto& dep : d) { if (std::hypot(cust->x - dep->x, cust->y - dep->y) < std::hypot(cust->x - cl->x, cust->y - cl->y)) cl = dep; }
        g[cl].push_back(cust);
    }
    return g;
}
std::vector<Route> RouteOptimizer::nearestMultiDepot(const std::vector<std::shared_ptr<Node>>& c, const std::vector<std::shared_ptr<Node>>& d) {
    auto g = assignCustomersToDepots(c, d); std::vector<Route> routes;
    for (const auto& dep : d) {
        auto rem = g[dep];
        while (!rem.empty() && (int)routes.size() < vehicleCount) {
            Route r; r.depot = dep; auto curr = dep;
            while (true) {
                std::shared_ptr<Node> near = nullptr; double best = std::numeric_limits<double>::max();
                for (const auto& cust : rem) { if (r.load() + cust->demand <= capacity && distance(curr, cust) < best) { near = cust; best = distance(curr, cust); } }
                if (!near) break;
                r.stops.push_back(near); rem.erase(std::remove(rem.begin(), rem.end(), near), rem.end()); curr = near;
            }
            if (!r.stops.empty()) routes.push_back(r);
        }
    }
    return routes;
}
std::vector<Route> RouteOptimizer::savingsMultiDepot(const std::vector<std::shared_ptr<Node>>& c, const std::vector<std::shared_ptr<Node>>& d) {
    auto g = assignCustomersToDepots(c, d); std::vector<Route> routes;
    for (const auto& dep : d) {
        const auto& grp = g[dep]; std::vector<Route> dRoutes; dRoutes.reserve(grp.size()); std::unordered_map<int, Route*> owner;
        for (const auto& cust : grp) {
            Route r; r.depot = dep; r.stops.push_back(cust); dRoutes.push_back(r); owner[cust->id] = &dRoutes.back();
        }
        std::vector<Saving> savings;
        for (size_t i = 0; i < grp.size(); ++i) {
            for (size_t j = i + 1; j < grp.size(); ++j) {
                double val = distance(dep, grp[i]) + distance(dep, grp[j]) - distance(grp[i], grp[j]); savings.emplace_back(grp[i], grp[j], val);
            }
        }
        std::sort(savings.begin(), savings.end(), [](const Saving& a, const Saving& b) { return a.value > b.value; });
        for (const auto& s : savings) {
            Route* r1 = owner[s.first->id]; Route* r2 = owner[s.second->id];
            if (!r1 || !r2 || r1 == r2 || r1->load() + r2->load() > capacity) continue;
            auto ms = mergeAtRouteEnds(*r1, *r2, s);
            if (!ms.empty()) { r1->stops = ms; for (const auto& st : r2->stops) owner[st->id] = r1; r2->stops.clear(); }
        }
        for (auto& r : dRoutes) { if (!r.stops.empty()) routes.push_back(r); }
    }
    while ((int)routes.size() > vehicleCount) {
        std::unique_ptr<MergeCandidate> best(findBestRouteMerge(routes, d)); if (!best) break;
        routes[best->firstIndex].stops = best->stops; routes[best->firstIndex].depot = best->depot; routes.erase(routes.begin() + best->secondIndex);
    }
    return routes;
}
std::vector<std::shared_ptr<Node>> RouteOptimizer::mergeAtRouteEnds(const Route& f, const Route& s, const Saving& sav) {
    auto fS = sav.first; auto sS = sav.second; bool fE = f.stops.back() == fS; bool fSt = f.stops.front() == fS; bool sE = s.stops.back() == sS; bool sSt = s.stops.front() == sS;
    std::vector<std::shared_ptr<Node>> m;
    if (fE && sSt) { m = f.stops; m.insert(m.end(), s.stops.begin(), s.stops.end()); }
    else if (fSt && sE) { m = s.stops; m.insert(m.end(), f.stops.begin(), f.stops.end()); }
    else if (fSt && sSt) { m = f.stops; std::reverse(m.begin(), m.end()); m.insert(m.end(), s.stops.begin(), s.stops.end()); }
    else if (fE && sE) { m = f.stops; auto rev = s.stops; std::reverse(rev.begin(), rev.end()); m.insert(m.end(), rev.begin(), rev.end()); }
    return m;
}
RouteOptimizer::MergeCandidate* RouteOptimizer::findBestRouteMerge(const std::vector<Route>& r, const std::vector<std::shared_ptr<Node>>& d) {
    double best = std::numeric_limits<double>::max(); MergeCandidate* bc = nullptr;
    for (size_t i = 0; i < r.size(); ++i) {
        for (size_t j = i + 1; j < r.size(); ++j) {
            if (r[i].load() + r[j].load() > capacity) continue;
            for (int rot = 0; rot < 4; ++rot) {
                auto s1 = r[i].stops; auto s2 = r[j].stops;
                if (rot & 1) std::reverse(s1.begin(), s1.end());
                if (rot & 2) std::reverse(s2.begin(), s2.end());
                s1.insert(s1.end(), s2.begin(), s2.end());
                for (const auto& dep : d) {
                    double sc = routeDistance(dep, s1); if (sc < best) { best = sc; delete bc; bc = new MergeCandidate((int)i, (int)j, dep, s1); }
                }
            }
        }
    }
    return bc;
}
void RouteOptimizer::twoOpt(Route& route) {
    if (route.stops.size() < 2) return;
    bool imp = true; int iter = 0;
    while (imp && iter++ < 100) {
        imp = false;
        for (size_t i = 0; i < route.stops.size() - 1; ++i) {
            for (size_t k = i + 1; k < route.stops.size(); ++k) {
                auto b = (i == 0) ? route.depot : route.stops[i - 1]; auto f = route.stops[i]; auto l = route.stops[k]; auto a = (k == route.stops.size() - 1) ? route.depot : route.stops[k + 1];
                if (distance(b, l) + distance(f, a) + TWO_OPT_EPSILON < distance(b, f) + distance(l, a)) { std::reverse(route.stops.begin() + i, route.stops.begin() + k + 1); imp = true; }
            }
        }
    }
}
void RouteOptimizer::threeOptLite(Route& route) {
    int sc = (int)route.stops.size(); if (sc < 5) return;
    bool imp = true; int r = 0;
    while (imp && r++ < 2) {
        imp = false;
        for (int i = 0; i < sc - 3; ++i) {
            for (int j = i + 1; j < sc - 2; ++j) {
                for (int k = j + 1; k < sc - 1; ++k) {
                    const auto original = route.stops;
                    auto best = original;
                    double bestDistance = routeDistance(route.depot, original);
                    for (const auto& range : {std::pair<int, int>{i, j}, {j, k}, {i, k}}) {
                        auto candidate = original;
                        std::reverse(candidate.begin() + range.first, candidate.begin() + range.second);
                        double candidateDistance = routeDistance(route.depot, candidate);
                        if (candidateDistance + THREE_OPT_EPSILON < bestDistance) {
                            bestDistance = candidateDistance;
                            best = std::move(candidate);
                        }
                    }
                    if (best != original) {
                        route.stops = std::move(best);
                        imp = true;
                        return;
                    }
                }
            }
        }
    }
}
double RouteOptimizer::routeMetric(const std::shared_ptr<Node>& d, const std::vector<std::shared_ptr<Node>>& s, Metric m) {
    if (!d || s.empty()) return 0.0;
    double total = metricValue(leg(d, s[0]), m);
    for (size_t i = 1; i < s.size(); ++i) total += metricValue(leg(s[i - 1], s[i]), m);
    total += metricValue(leg(s.back(), d), m); return total;
}
double RouteOptimizer::metricValue(const Leg& l, Metric m) const {
    if (m == Metric::DISTANCE) return l.km;
    if (m == Metric::TIME) return l.minutes;
    if (m == Metric::COST) return l.cost;
    return l.score;
}
double RouteOptimizer::routeDistance(const std::shared_ptr<Node>& d, const std::vector<std::shared_ptr<Node>>& s) { return routeMetric(d, s, Metric::SCORE); }
double RouteOptimizer::routeKm(const std::shared_ptr<Node>& d, const std::vector<std::shared_ptr<Node>>& s) { return routeMetric(d, s, Metric::DISTANCE); }
double RouteOptimizer::routeMinutes(const std::shared_ptr<Node>& d, const std::vector<std::shared_ptr<Node>>& s) { return routeMetric(d, s, Metric::TIME); }
double RouteOptimizer::routeCost(const std::shared_ptr<Node>& d, const std::vector<std::shared_ptr<Node>>& s) { return routeMetric(d, s, Metric::COST); }
Leg RouteOptimizer::shortestLeg(const std::shared_ptr<RoadNode>& start, const std::shared_ptr<RoadNode>& target) {
    Leg res; if (start == target) { res.path.push_back(start); return res; }
    std::unordered_map<std::shared_ptr<RoadNode>, double> dists; std::unordered_map<std::shared_ptr<RoadNode>, std::shared_ptr<RoadNode>> prev;
    auto cmp = [&](const std::shared_ptr<RoadNode>& a, const std::shared_ptr<RoadNode>& b) { return dists[a] > dists[b]; };
    std::priority_queue<std::shared_ptr<RoadNode>, std::vector<std::shared_ptr<RoadNode>>, decltype(cmp)> q(cmp);
    std::set<std::shared_ptr<RoadNode>> setld; dists[start] = 0.0; q.push(start);
    while (!q.empty()) {
        auto curr = q.top(); q.pop(); if (!setld.insert(curr).second) continue; if (curr == target) break;
        for (const auto& e : curr->edges) {
            auto next = e.to.lock();
            if (!next) continue;
            double cD = dists[curr] + objectiveScore(e.km, e.minutes, e.cost);
            if (dists.find(next) == dists.end() || cD < dists[next]) { dists[next] = cD; prev[next] = curr; q.push(next); }
        }
    }
    if (dists.find(target) == dists.end()) {
        res.km = geoKm(start->lat, start->lon, target->lat, target->lon); res.minutes = res.km * 2.0; res.cost = res.km * 12.0;
        res.score = objectiveScore(res.km, res.minutes, res.cost); res.path = {start, target}; return res;
    }
    auto c = target; while (c) { res.path.push_back(c); if (c == start) break; c = prev[c]; }
    std::reverse(res.path.begin(), res.path.end());
    for (size_t i = 0; i < res.path.size() - 1; ++i) {
        for (const auto& e : res.path[i]->edges) {
            if (e.to.lock() == res.path[i + 1]) { res.km += e.km; res.minutes += e.minutes; res.cost += e.cost; break; }
        }
    }
    res.score = objectiveScore(res.km, res.minutes, res.cost); return res;
}
void RouteOptimizer::retainLargestStronglyConnectedComponent(std::set<std::shared_ptr<RoadNode>>& used) {
    std::vector<std::shared_ptr<RoadNode>> nodes(used.begin(), used.end()); std::unordered_map<std::shared_ptr<RoadNode>, std::vector<std::shared_ptr<RoadNode>>> rev;
    for (const auto& n : nodes) {
        for (const auto& e : n->edges) {
            if (auto target = e.to.lock()) rev[target].push_back(n);
        }
    }
    std::set<std::shared_ptr<RoadNode>> vis; std::stack<std::shared_ptr<RoadNode>> fin;
    for (const auto& root : nodes) {
        if (!vis.insert(root).second) continue;
        std::stack<TraversalFrame> st;
        st.emplace(root);
        while (!st.empty()) {
            auto& f = st.top();
            if (f.nextEdge < f.node->edges.size()) {
                auto nxt = f.node->edges[f.nextEdge++].to.lock();
                if (nxt && used.count(nxt) && vis.insert(nxt).second) st.emplace(nxt);
            }
            else { fin.push(f.node); st.pop(); }
        }
    }
    std::set<std::shared_ptr<RoadNode>> assgd; std::set<std::shared_ptr<RoadNode>> lComp;
    while (!fin.empty()) {
        auto root = fin.top(); fin.pop(); if (!assgd.insert(root).second) continue;
        std::set<std::shared_ptr<RoadNode>> comp; std::stack<std::shared_ptr<RoadNode>> st; st.push(root);
        while (!st.empty()) { auto n = st.top(); st.pop(); comp.insert(n); for (const auto& p : rev[n]) { if (assgd.insert(p).second) st.push(p); } }
        if (comp.size() > lComp.size()) lComp = comp;
    }
    for (auto it = used.begin(); it != used.end();) { if (!lComp.count(*it)) it = used.erase(it); else ++it; }
    for (const auto& n : used) {
        n->edges.erase(std::remove_if(n->edges.begin(), n->edges.end(), [&](const RoadEdge& edge) {
            auto target = edge.to.lock();
            return !target || !used.count(target);
        }), n->edges.end());
    }
}
double RouteOptimizer::geoKm(double lat1, double lon1, double lat2, double lon2) {
    constexpr double pi = 3.14159265358979323846;
    double r = 6371.0; double dLat = (lat2 - lat1) * pi / 180.0; double dLon = (lon2 - lon1) * pi / 180.0;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) + std::cos(lat1 * pi / 180.0) * std::cos(lat2 * pi / 180.0) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * r * std::asin(std::sqrt(a));
}