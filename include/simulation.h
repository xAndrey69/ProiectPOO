#ifndef SIMULATION_H
#define SIMULATION_H

#include "config.h"
#include "map.h"
#include "agents.h"
#include "hivemind.h"
#include <vector>
#include <fstream>
#include <string>
#include <memory> // Pentru unique_ptr

class Simulation {
private:
    // Folosim unique_ptr pentru management automat de memorie
    std::unique_ptr<Map> map;
    std::vector<std::unique_ptr<Agent>> agents;
    std::vector<std::unique_ptr<Package>> packages;
    std::unique_ptr<HiveMind> hiveMind;
    std::unique_ptr<ProceduralMapGenerator> mapGenerator;
    
    // Timp și statistici
    int currentTick;
    int totalTicks;
    
    // Statistici financiare
    long long totalRevenue;
    long long totalCosts;
    long long totalPenalties;
    
    // Statistici operaționale
    int packagesDelivered;
    int packagesFailed;
    int agentsLost;
    int agentsAlive;
    
    // Logging
    std::ofstream logFile;
    bool enableLogging;
    
    // Metode private
    void initializeSimulation();
    void generateInitialAgents();
    void spawnPackages();
    void updateAgents();
    void processDeliveries();
    void checkAgentStatus();
    void logEvent(const std::string& message);
    void saveStatistics();
    
public:
    Simulation(bool enableLog = false);
    ~Simulation();
    
    // Metode principale
    void initialize();
    void run();
    void printFinalReport() const;
    
    // Getters pentru statistici
    long long getTotalProfit() const { return totalRevenue - totalCosts - totalPenalties; }
    int getPackagesDelivered() const { return packagesDelivered; }
    int getAgentsLost() const { return agentsLost; }
    double getSuccessRate() const { 
        return packages.empty() ? 0.0 : (packagesDelivered * 100.0) / packages.size(); 
    }
    int getAgentsAlive() const { return agentsAlive; }
};

#endif
