#include "map.h"
#include "config.h"
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>

void Map::init(int h, int w) {
    height = h;
    width = w;
    grid.clear();
    clients.clear();
    stations.clear();
    for (int i = 0; i < h; i++) grid.push_back(std::string(w, CELL_EMPTY));
}

void Map::setCell(int x, int y, char type) {
    if (isValidCoord(x, y)) {
        grid[y][x] = type;
        if (type == CELL_BASE) { startX = x; startY = y; }
        if (type == CELL_CLIENT) clients.push_back({x, y});
        if (type == CELL_STATION) stations.push_back({x, y});
    }
}

char Map::getCell(int x, int y) const {
    if (!isValidCoord(x, y)) return CELL_WALL;
    return grid[y][x];
}

bool Map::isValidCoord(int x, int y) const {
    return x >= 0 && x < width && y >= 0 && y < height;
}

void Map::print() const {
    for (const auto& row : grid) std::cout << row << "\n";
}

int ProceduralMapGenerator::getRandom(int min, int max) {
    thread_local static std::mt19937
    rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

bool ProceduralMapGenerator::validateMap(const Map& map) {
    int h = map.getHeight();
    int w = map.getWidth();
    std::vector<std::vector<bool>> visited(h, std::vector<bool>(w, false));
    std::queue<Point> q;
    
    q.push({map.startX, map.startY});
    visited[map.startY][map.startX] = true;
    
    int targetsFound = 0;
    int totalTargets = map.clients.size() + map.stations.size();
    
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};

    while (!q.empty()) {
        Point curr = q.front();
        q.pop();

        char cell = map.getCell(curr.x, curr.y);
        if (cell == CELL_CLIENT || cell == CELL_STATION) targetsFound++;

        for (int i = 0; i < 4; i++) {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];
            if (map.isValidCoord(nx, ny) && !visited[ny][nx] && map.getCell(nx, ny) != CELL_WALL) {
                visited[ny][nx] = true;
                q.push({nx, ny});
            }
        }
    }
    return targetsFound == totalTargets;
}

void ProceduralMapGenerator::generate(Map& map) {
    Config* cfg = Config::getInstance();
    bool valid = false;
    int attempts = 0;
    
    while (!valid && attempts < 2000) {
        attempts++;
        map.init(cfg->mapHeight, cfg->mapWidth);
        
        map.setCell(getRandom(0, cfg->mapWidth-1), getRandom(0, cfg->mapHeight-1), CELL_BASE);
        
        for (int i=0; i<cfg->clientsCount; i++) {
             int x, y;
             do { x = getRandom(0, cfg->mapWidth-1); y = getRandom(0, cfg->mapHeight-1); } 
             while (map.getCell(x,y) != CELL_EMPTY);
             map.setCell(x, y, CELL_CLIENT);
        }
        for (int i=0; i<cfg->maxStations; i++) {
             int x, y;
             do { x = getRandom(0, cfg->mapWidth-1); y = getRandom(0, cfg->mapHeight-1); } 
             while (map.getCell(x,y) != CELL_EMPTY);
             map.setCell(x, y, CELL_STATION);
        }

        int walls = (cfg->mapHeight * cfg->mapWidth) * 0.2;
        for (int i=0; i<walls; i++) {
            int x = getRandom(0, cfg->mapWidth-1);
            int y = getRandom(0, cfg->mapHeight-1);
            if (map.getCell(x, y) == CELL_EMPTY) map.setCell(x, y, CELL_WALL);
        }

        if (validateMap(map)) valid = true;
    }

    if (!valid) {
	throw std::runtime_error("Eroare: Harta invalida dupa multiple incercari.");
    }
}
