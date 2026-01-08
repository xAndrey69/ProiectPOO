#ifndef MAP_H
#define MAP_H

#include <vector>
#include <string>
#include "utils.h"

#define CELL_EMPTY   '.'
#define CELL_WALL    '#'
#define CELL_BASE    'B'
#define CELL_STATION 'S'
#define CELL_CLIENT  'D'

class Map {
private:
    int height, width;
    std::vector<std::string> grid;

public:
    int startX, startY; 
    std::vector<Point> clients;
    std::vector<Point> stations;

    Map() : height(0), width(0), startX(0), startY(0) {}
    
    void init(int h, int w);
    void setCell(int x, int y, char type);
    char getCell(int x, int y) const;
    bool isValidCoord(int x, int y) const;
    void print() const;
    
    int getHeight() const { return height; }
    int getWidth() const { return width; }
    const std::vector<Point>& getClients() const { return clients; }
    const std::vector<Point>& getStations() const { return stations; }
    Point getBasePosition() const { return {startX, startY}; }
};

class IMapGenerator {
public:
    virtual void generate(Map& map) = 0;
    virtual ~IMapGenerator() {}
};

class ProceduralMapGenerator : public IMapGenerator {
public:
    void generate(Map& map) override;
    
private:
    bool validateMap(const Map& map);
    int getRandom(int min, int max);
};

#endif
