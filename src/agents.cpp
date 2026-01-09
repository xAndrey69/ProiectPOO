#include "agents.h"
#include "map.h"
#include "hivemind.h" 
#include <queue>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <memory>

using namespace std;

static Point findNextStepBFS(const Point& start, const Point& target, const Map& map) {
    if (start == target) return start;

    int h = map.getHeight();
    int w = map.getWidth();
    int area = h * w;

    static thread_local std::vector<int> visited;
    static thread_local std::vector<int> parent;
    static thread_local std::vector<int> q_vec;
    static thread_local int runToken = 0;
    
    if ((int)visited.size() != area) {
        visited.assign(area, 0);
        parent.assign(area, -1);
        q_vec.resize(area); 
    }

    runToken++;
    if (runToken == 0) { 
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
      state(IDLE), currentPackage(nullptr) {
	  hasPhysicalPackage = false;
      }

void Agent::charge() {
    if (state == CHARGING || state == IDLE) {
        battery += maxBattery * 0.25f; 
        if (battery > maxBattery) {
            battery = maxBattery;
        }
    }
}

void Agent::assignTask(Package* pkg, Point dest) {
    currentPackage = pkg;
    hasPhysicalPackage = false;
    target = dest;
    state = MOVING;
}

void Agent::sendToCharge(Point station) {
    target = station;
    state = MOVING;
   
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

Drone::Drone(int id, int x, int y) 
    : Agent(id, x, y, DRONE, 100.0f, 10.0f, 15) {}

void Drone::move(const Map& map) {
    (void)map;

    battery -= consumption;
    if (battery <= 0) {
        battery = 0;
        state = DEAD;
        return;
    }
    
    if (state != MOVING) return;
    
    int speed = static_cast<int>(getSpeed());
    
    for (int i = 0; i < speed && position != target; i++) {
        if (position.x < target.x) position.x++;
        else if (position.x > target.x) position.x--;
        else if (position.y < target.y) position.y++;
        else if (position.y > target.y) position.y--;
    }
    
    if (position == target) {
	if (currentPackage != nullptr && !hasPhysicalPackage) {
            if (position == map.getBasePosition()) {
                hasPhysicalPackage = true;        
                target = currentPackage->destCoord; 
            }
        }
        
        else if (currentPackage != nullptr && hasPhysicalPackage) {
            state = IDLE;
        } else {
            state = IDLE;
        }
    }
}

Robot::Robot(int id, int x, int y) 
    : Agent(id, x, y, ROBOT, 300.0f, 2.0f, 1) {}

void Robot::move(const Map& map) {
    
    battery -= consumption;
    if (battery <= 0) {
        battery = 0;
        state = DEAD;
        return;
    }

    if (state != MOVING) return;
    
    if (position != target) {
        Point nextStep = findNextStepBFS(position, target, map);
        position = nextStep;
    }
    
    
    if (position == target) {
	if (currentPackage != nullptr && !hasPhysicalPackage) {
            if (position == map.getBasePosition()) {
                hasPhysicalPackage = true;        
                target = currentPackage->destCoord; 
            }
        }
        
        else if (currentPackage != nullptr && hasPhysicalPackage) {
            state = IDLE;
        } else {
            state = IDLE;
        }
    }
}

Scooter::Scooter(int id, int x, int y) 
    : Agent(id, x, y, SCOOTER, 200.0f, 5.0f, 4) {}

void Scooter::move(const Map& map) {
    
    battery -= consumption;
    if (battery <= 0) {
        battery = 0;
        state = DEAD;
        return;
    }
    
    if (state != MOVING) return;

    int speed = static_cast<int>(getSpeed());
    
    for (int i = 0; i < speed && position != target; i++) {
        Point nextStep = findNextStepBFS(position, target, map);
        position = nextStep;
    }
    

    if (position == target) {
	if (currentPackage != nullptr && !hasPhysicalPackage) {
            if (position == map.getBasePosition()) {
                hasPhysicalPackage = true;        
                target = currentPackage->destCoord; 
            }
        }
        
        else if (currentPackage != nullptr && hasPhysicalPackage) {
            state = IDLE;
        } else {
            state = IDLE;
        }
    }
}

unique_ptr<Agent> AgentFactory::create(AgentType type, int id, int x, int y) {
    switch (type) {
        case DRONE:
            return unique_ptr<Agent>(new Drone(id, x, y));
        case ROBOT:
            return unique_ptr<Agent>(new Robot(id, x, y));
        case SCOOTER:
            return unique_ptr<Agent>(new Scooter(id, x, y));
        default:
            throw std::invalid_argument("AgentFactory: nu s-a gasit tipul de agent");
    }
}
