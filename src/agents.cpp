#include "agents.h"
#include "map.h"
#include "hivemind.h" // Pentru struct Package
#include <queue>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <memory>

using namespace std;

// Helper pentru BFS pathfinding
// static Point findNextStepBFS(const Point& start, const Point& target, const Map& map) {
//     if (start == target) return start;
    
//     int h = map.getHeight();
//     int w = map.getWidth();
    
//     vector<vector<bool>> visited(h, vector<bool>(w, false));
//     vector<vector<Point>> parent(h, vector<Point>(w, {-1, -1}));
//     queue<Point> q;
    
//     q.push(start);
//     visited[start.y][start.x] = true;
    
//     // Direcții: sus, jos, stânga, dreapta
//     const int dx[] = {0, 0, -1, 1};
//     const int dy[] = {-1, 1, 0, 0};
    
//     while (!q.empty()) {
//         Point current = q.front();
//         q.pop();
        
//         if (current == target) {
//             // Reconstruim calea până la primul pas
//             Point step = target;
//             while (parent[step.y][step.x] != start && parent[step.y][step.x].x != -1) {
//                 step = parent[step.y][step.x];
//             }
//             return (step == start) ? target : step;
//         }
        
//         for (int i = 0; i < 4; i++) {
//             int nx = current.x + dx[i];
//             int ny = current.y + dy[i];
            
//             if (nx >= 0 && nx < w && ny >= 0 && ny < h && !visited[ny][nx]) {
//                 char cell = map.getCell(nx, ny);
//                 if (cell != CELL_WALL) {
//                     visited[ny][nx] = true;
//                     parent[ny][nx] = current;
//                     q.push({nx, ny});
//                 }
//             }
//         }
//     }
    
//     // Niciun drum găsit - rămâne pe loc
//     return start;
// }

static Point findNextStepBFS(const Point& start, const Point& target, const Map& map) {
    if (start == target) return start;

    int h = map.getHeight();
    int w = map.getWidth();
    int area = h * w;

    // 1. MEMORIE STATICA PER THREAD (Nu se aloca/dezaloca niciodata dupa prima rulare)
    // Folosim int in loc de bool pentru a folosi un "token" de rulare
    // thread_local inseamna ca fiecare din cele 24 de nuclee are propriul vector
    static thread_local std::vector<int> visited;
    static thread_local std::vector<int> parent;
    static thread_local std::vector<int> q_vec; // Folosim vector pe post de queue (mai rapid)
    static thread_local int runToken = 0; // Contor pentru a nu mai face "clear"

    // Initializare doar la prima rulare (sau daca se schimba harta)
    if ((int)visited.size() != area) {
        visited.assign(area, 0);
        parent.assign(area, -1);
        q_vec.resize(area); // Pre-alocam marimea maxima posibila
    }

    // Incrementam token-ul. Orice celula cu visited[i] == runToken e "vizitata" in tura asta.
    runToken++;
    if (runToken == 0) { // Overflow protection (foarte rar)
        std::fill(visited.begin(), visited.end(), 0);
        runToken = 1;
    }

    // Helper lambda rapid pentru indexare 1D
    auto toIdx = [&](int x, int y) { return y * w + x; };
    auto toPoint = [&](int idx) { return Point{idx % w, idx / w}; };

    int startIdx = toIdx(start.x, start.y);
    int targetIdx = toIdx(target.x, target.y);

    // Setup Queue manual (fara std::queue care aloca memorie)
    int head = 0;
    int tail = 0;

    q_vec[tail++] = startIdx;
    visited[startIdx] = runToken;
    parent[startIdx] = -1; // Marcam startul

    const int dx[] = {0, 0, -1, 1};
    const int dy[] = {-1, 1, 0, 0};

    bool found = false;

    // BFS Loop (Foarte rapid, acces direct la memorie)
    while(head < tail) {
        int currentIdx = q_vec[head++];

        if (currentIdx == targetIdx) {
            found = true;
            break;
        }

        Point current = toPoint(currentIdx);

        for(int i=0; i<4; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            // Verificare limite manuala
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                // Accesam gridul. Nota: getCell e rapid, dar daca ai acces direct la vectorul hartii e si mai bine
                if (map.getCell(nx, ny) != CELL_WALL) {
                    int nIdx = toIdx(nx, ny);

                    // Verificam token-ul in loc de bool
                    if (visited[nIdx] != runToken) {
                        visited[nIdx] = runToken;
                        parent[nIdx] = currentIdx;
                        q_vec[tail++] = nIdx;
                    }
                }
            }
        }
    }

    if (found) {
        // Reconstruim calea invers
        int curr = targetIdx;
        int prev = -1;
        // Mergem inapoi pana la start
        while (curr != startIdx) {
            prev = curr;
            curr = parent[curr];
        }
        return toPoint(prev); // Returnam primul pas
    }

    return start; // Nu exista drum, stam pe loc
}
// Implementare Agent
Agent::Agent(int _id, int x, int y, AgentType _type, 
             float _maxBattery, float _consumption, int _costPerTick)
    : id(_id), type(_type), position({x, y}), target({x, y}),
      battery(_maxBattery), maxBattery(_maxBattery), 
      consumption(_consumption), costPerTick(_costPerTick),
      state(IDLE), currentPackage(nullptr) {}

void Agent::charge() {
    if (state == CHARGING || state == IDLE) {
        battery += maxBattery * 0.25f; // 25% per tick
        if (battery > maxBattery) {
            battery = maxBattery;
        }
    }
}

void Agent::assignTask(Package* pkg, Point dest) {
    currentPackage = pkg;
    target = dest;
    state = MOVING;
}

void Agent::sendToCharge(Point station) {
    target = station;
    state = MOVING;
    // Dacă avea pachet, îl anulăm temporar
    if (currentPackage) {
        currentPackage->assigned = false;
        currentPackage = nullptr;
    }
}

void Agent::dropPackage() {
    currentPackage = nullptr;
    state = IDLE;
}

void Agent::updatePosition(Point newPos) {
    position = newPos;
}

// Implementare Drone
Drone::Drone(int id, int x, int y) 
    : Agent(id, x, y, DRONE, 100.0f, 10.0f, 15) {}

void Drone::move(const Map& map) {
    (void)map;
    
    if (state != MOVING) return;
    //nu e nevoie de harta la drona, ca zboara peste orice
    // Consumă baterie
    battery -= consumption;
    if (battery <= 0) {
        battery = 0;
        state = DEAD;
        return;
    }
    
    // Drona se mișcă în linie dreaptă, ignorând obstacolele
    int speed = static_cast<int>(getSpeed());
    
    for (int i = 0; i < speed && position != target; i++) {
        if (position.x < target.x) position.x++;
        else if (position.x > target.x) position.x--;
        else if (position.y < target.y) position.y++;
        else if (position.y > target.y) position.y--;
    }
    
    // Verifică dacă a ajuns la destinație
    if (position == target) {
        state = IDLE;
    }
}

// Implementare Robot
Robot::Robot(int id, int x, int y) 
    : Agent(id, x, y, ROBOT, 300.0f, 2.0f, 1) {}

void Robot::move(const Map& map) {
    if (state != MOVING) return;
    
    // Consumă baterie
    battery -= consumption;
    if (battery <= 0) {
        battery = 0;
        state = DEAD;
        return;
    }
    
    // Robotul folosește pathfinding pentru a evita zidurile
    if (position != target) {
        Point nextStep = findNextStepBFS(position, target, map);
        position = nextStep;
    }
    
    // Verifică dacă a ajuns la destinație
    if (position == target) {
        state = IDLE;
    }
}

// Implementare Scooter
Scooter::Scooter(int id, int x, int y) 
    : Agent(id, x, y, SCOOTER, 200.0f, 5.0f, 4) {}

void Scooter::move(const Map& map) {
    if (state != MOVING) return;
    
    // Consumă baterie
    battery -= consumption;
    if (battery <= 0) {
        battery = 0;
        state = DEAD;
        return;
    }
    
    // Scooterul se mișcă cu viteza 2, folosind pathfinding
    int speed = static_cast<int>(getSpeed());
    
    for (int i = 0; i < speed && position != target; i++) {
        Point nextStep = findNextStepBFS(position, target, map);
        position = nextStep;
    }
    
    // Verifică dacă a ajuns la destinație
    if (position == target) {
        state = IDLE;
    }
}

// Implementare AgentFactory
unique_ptr<Agent> AgentFactory::create(AgentType type, int id, int x, int y) {
    switch (type) {
        case DRONE:
            return unique_ptr<Agent>(new Drone(id, x, y));
        case ROBOT:
            return unique_ptr<Agent>(new Robot(id, x, y));
        case SCOOTER:
            return unique_ptr<Agent>(new Scooter(id, x, y));
        default:
            return nullptr;
    }
}
