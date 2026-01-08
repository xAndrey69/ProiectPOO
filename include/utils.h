#ifndef UTILS_H
#define UTILS_H

#include <cmath>
#include <cstdlib>

struct Point {
    int x, y;

    bool operator==(const Point& other) const { 
        return x == other.x && y == other.y; 
    }
    
    bool operator!=(const Point& other) const { 
        return !(*this == other); 
    }
    
    // Distanța Manhattan (pentru grid)
    static int distance(const Point& a, const Point& b) {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    }
};

#endif
