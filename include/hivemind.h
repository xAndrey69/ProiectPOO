#ifndef HIVEMIND_H
#define HIVEMIND_H

#include "utils.h"
#include <vector>
#include <memory>

// Forward declarations
class Map;
class Agent;

// Structura pentru pachete
struct Package {
    int id;
    Point destCoord;
    int reward;
    int deadline;
    int spawnTick;
    bool assigned;
    bool delivered;
    int clientId;
    
    Package(int _id, const Point& _dest, int _reward, int _deadline, 
            int _spawnTick, int _clientId)
        : id(_id), destCoord(_dest), reward(_reward), deadline(_deadline),
          spawnTick(_spawnTick), assigned(false), delivered(false),
          clientId(_clientId) {}
    
    // Calculează dacă pachetul este întârziat
    bool isLate(int currentTick) const {
        return currentTick > deadline;
    }
    
    // Calculează întârzierea în ticks
    int getDelay(int currentTick) const {
        return isLate(currentTick) ? currentTick - deadline : 0;
    }

    int getDelayPenalty(int currentTick) const {
        return isLate(currentTick) ? 50 : 0;
    }

    int getFailurePenalty() const {
        return delivered ? 0 : 200;
    }
};

class HiveMind {
private:
    // Structură internă pentru scorul atribuirilor
    struct AssignmentScore {
        Agent* agent;
        Package* package;
        double score;
        double estimatedProfit;
        int estimatedDeliveryTime;
        int energyRisk;
        
        AssignmentScore(Agent* a, Package* p, double s, double ep, int edt, int er)
            : agent(a), package(p), score(s), estimatedProfit(ep), 
              estimatedDeliveryTime(edt), energyRisk(er) {}
        
        bool operator<(const AssignmentScore& other) const {
            return score < other.score;
        }
    };
    
   
    struct OptimizationParams {
        double profitWeight = 0.50;      // Importanța profitului imediat
        double safetyWeight = 0.30;      // Importanța siguranței bateriei
        double urgencyWeight = 0.20;     // Importanța deadline-ului
        double distanceWeight = 0.10;    // Importanța distanței
        
        int criticalBatteryThreshold = 20;   // Baterie critică (%)
        int lowBatteryThreshold = 40;        // Baterie scăzută (%)
        int safeBatteryMargin = 30;          // Marja de siguranță (%)
    };
    
    OptimizationParams params;
    
    // Metode helper private
    Point findNearestChargingPoint(const Point& position, const Map& map) const;
    bool needsCharging(const Agent* agent, const Point& destination, const Map& map) const;
    int estimateDeliveryTime(const Agent* agent, const Point& destination) const;
    double estimateDeliveryCost(const Agent* agent, int deliveryTime) const;
    double calculateAssignmentScore(Agent* agent, Package* package, 
                                   const Map& map, int currentTick) const;
    
    // Strategii specifice
    void handleLowBatteryAgents(std::vector<Agent*>& agents, const Map& map);
    void assignPackages(std::vector<Agent*>& agents, std::vector<Package*>& packages,
                       const Map& map, int currentTick);
    void optimizeIdleAgents(std::vector<Agent*>& agents, const Map& map);
    
public:
    HiveMind() = default;
    
    // Setează parametrii de optimizare
    void setOptimizationParams(const OptimizationParams& newParams) {
        params = newParams;
    }
    
    // Metoda principală - apelată la fiecare tick
    void update(std::vector<Agent*>& agents, std::vector<Package*>& packages,
                const Map& map, int currentTick);
    
    // Getter pentru parametri
    const OptimizationParams& getParams() const { return params; }
};

#endif
