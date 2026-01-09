#include "simulation.h"
#include <iostream>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <stdexcept>

using namespace std;

Simulation::Simulation(bool enableLog) 
    : currentTick(0), totalTicks(0),
      totalRevenue(0), totalCosts(0), totalPenalties(0),
      packagesDelivered(0), packagesFailed(0),
      agentsLost(0), agentsAlive(0),
      enableLogging(enableLog) {
    
    map.reset(new Map());
    hiveMind.reset(new HiveMind());
    mapGenerator.reset(new ProceduralMapGenerator());
    
    if (enableLogging) {
        logFile.open("simulation_log.txt");
    }
}

Simulation::~Simulation() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Simulation::logEvent(const string& message) {
    if (enableLogging && logFile.is_open()) {
        logFile << "[TICK " << currentTick << "] " << message << endl;
    }
}

void Simulation::initialize() {
    Config* config = Config::getInstance();
    
    logEvent("=== INITIALIZARE SIMULARE ===");
    
    mapGenerator->generate(*map);
    totalTicks = config->maxTicks;
    
    generateInitialAgents();
    
    logEvent("Simularea este gata să înceapă.");
}

void Simulation::generateInitialAgents() {
    Config* config = Config::getInstance();
    Point basePos = map->getBasePosition();
    
    int agentId = 0;
    
    for (int i = 0; i < config->dronesCount; i++) {
        agents.push_back(std::unique_ptr<Agent>(AgentFactory::create(DRONE, agentId++, basePos.x, basePos.y)));
    }
    
    for (int i = 0; i < config->robotsCount; i++) {
        agents.push_back(std::unique_ptr<Agent>(AgentFactory::create(ROBOT, agentId++, basePos.x, basePos.y)));
    }
    
    for (int i = 0; i < config->scootersCount; i++) {
        agents.push_back(std::unique_ptr<Agent>(AgentFactory::create(SCOOTER, agentId++, basePos.x, basePos.y)));
    }
    
    agentsAlive = agents.size();
    logEvent("Creati " + to_string(agentsAlive) + " agenti initiali.");
}

void Simulation::spawnPackages() {
    Config* config = Config::getInstance();
    
    // Verifică dacă trebuie să genereze pachete
    if (currentTick % config->spawnFrequency != 0) return;
    if ((int)packages.size() >= config->totalPackages) return;
    
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    static uniform_int_distribution<int> rewardDist(200, 800);
    static uniform_int_distribution<int> deadlineDist(10, 20);
    
    const vector<Point>& mapClients = map->getClients();
    if (mapClients.empty()) return;
    
    uniform_int_distribution<int> clientDist(0, (int)mapClients.size() - 1);
    int clientIdx = clientDist(gen);
    
    auto newPackage = std::unique_ptr<Package>(new Package(
        (int)packages.size(),
        mapClients[clientIdx],
        rewardDist(gen),
        currentTick + deadlineDist(gen),
        currentTick,
        clientIdx
    ));
    
    packages.push_back(std::move(newPackage));
    
    logEvent("Generat pachet " + to_string(packages.back()->id) + 
             " cu reward " + to_string(packages.back()->reward) +
             " si deadline la tick " + to_string(packages.back()->deadline));
}

void Simulation::updateAgents() {
    for (auto& agent : agents) {
        if (!agent->isAlive()) continue;
        
        Point pos = agent->getPosition();
        char cell = map->getCell(pos.x, pos.y);
        
        bool onChargingCell = (cell == CELL_BASE || cell == CELL_STATION);

        // //debug pt drone
	// if (agent->getType() == DRONE) {
        //     double distToTarget = 0.0;
        //     string statusTarget = "Idle";

        //     // Calculam distanta daca are un pachet (targetul e destinatia pachetului)
        //     if (agent->getPackage()) {
        //         Point dest = agent->getPackage()->destCoord;
        //         // Distanta Euclidiana
        //         distToTarget = std::hypot(dest.x - pos.x, dest.y - pos.y);
        //         statusTarget = "Livrare Pachet " + to_string(agent->getPackage()->id);
        //     } 
        //     // Optional: Daca nu are pachet dar se misca spre statie, ai putea calcula distanta pana acolo
        //     // dar momentan ne concentram pe pachet.

        //     // Construim mesajul de telemetrie
        //     // Format: [DRONE ID] Bat: 85.5% | Pos: (10, 20) | Target: 45.2m
            
        //     // Nota: Logarea la fiecare tick va genera un fisier foarte mare!
        //     // Folosim string stream pentru formatare frumoasa a float-urilor
        //     std::stringstream ss;
        //     ss << "[TELEMETRIE DRONA " << agent->getId() << "] "
        //        << "Bat: " << std::fixed << std::setprecision(1) << agent->getBatteryPercentage() << "% | "
        //        << "Pos: (" << pos.x << "," << pos.y << ") | "
        //        << "Status: " << statusTarget;
            
        //     if (agent->getPackage()) {
        //         ss << " | Dist. Dest: " << std::fixed << std::setprecision(2) << distToTarget;
        //     }

        //     logEvent(ss.str());
        // }
	
	//taxam daca e in miscare sau daca e idle
        if (!onChargingCell || agent->getState() == MOVING) {
            totalCosts += agent->getOperationalCost();
        }
        
        if (onChargingCell && agent->getState() != MOVING) {
            
            if (agent->getBatteryPercentage() < 100.0f) {

		//debug pt drone
		// if (agent->getType() == DRONE && agent->getState() != CHARGING) {
                //     std::stringstream ssCharge;
                //     ssCharge << "[INFO] Drona " << agent->getId() 
                //              << " a ajuns la statie si INCEPE INCARCAREA. (Baterie curenta: " 
                //              << std::fixed << std::setprecision(1) << agent->getBatteryPercentage() << "%)";
                //     logEvent(ssCharge.str());
                // }
		
                agent->setState(CHARGING); 
                agent->charge();
                
            } else {
                // Bateria e la 100%
		//debug drone
		// if (agent->getType() == DRONE && agent->getState() == CHARGING) {
                //      logEvent("[INFO] Drona " + to_string(agent->getId()) + " a TERMINAT incarcarea (100%).");
                // }
                agent->setState(IDLE);
            }
            
        } else {
            // state = MOVING sau IDLE dar nu pe charging cell
            float batteryBefore = agent->getBattery();
            agent->move(*map);
           
            if (!agent->isAlive() && batteryBefore > 0) {

		string typeStr;
                switch(agent->getType()) {
                    case DRONE: typeStr = "DRONE"; break;
                    case ROBOT: typeStr = "ROBOT"; break;
                    case SCOOTER: typeStr = "SCOOTER"; break;
                    default: typeStr = "NECUNOSCUT"; break;
                }


                Point deathPos = agent->getPosition();
                logEvent("!!! DECES AGENT !!! ID: " + to_string(agent->getId()) + 
                         " [" + typeStr + "] a murit la coordonatele (" + 
                         to_string(deathPos.x) + ", " + to_string(deathPos.y) + 
                         "). Baterie epuizata.");
			 
                agentsLost++;
                agentsAlive--;
                totalPenalties += 500;
                if (agent->isBusy()) {
                    Package* pkg = agent->getPackage();
                    if (pkg) { 
                        pkg->assigned = false; 
                        agent->dropPackage(); 
                    }
                }
            }
        }
    }
}

void Simulation::processDeliveries() {
    for (auto& agent : agents) {
	
        if (!agent->isAlive() || !agent->isBusy()) continue;
        
        Package* package = agent->getPackage();
        if (!package) continue;
        
        if (agent->getPosition() == package->destCoord) {
            package->delivered = true;
            packagesDelivered++;
            totalRevenue += package->reward;

	    string deliveryMsg = "Pachet " + to_string(package->id) + 
                                 " RECEPTIONAT de client. Livrat de Agent " + 
                                 to_string(agent->getId()) + " [" + 
                                 (agent->getType() == DRONE ? "DRONA" : 
                                  (agent->getType() == ROBOT ? "ROBOT" : "SCUTER")) + "]";
            
            if (currentTick > package->deadline) {
		totalPenalties += 50;
		int delay = currentTick - package->deadline;
                logEvent(deliveryMsg + " cu intarziere (" + to_string(delay) + 
                        " ticks). Penalizare: 50 credite");
            } else {
                logEvent(deliveryMsg + " la timp.");
            }
            
            agent->dropPackage();
        }
    }
}

void Simulation::checkAgentStatus() {
    agentsAlive = 0;
    for (auto& agent : agents) {
        if (agent->isAlive()) {
            agentsAlive++;
        }
    }
}

void Simulation::run() {
    Config* config = Config::getInstance();
    
    logEvent("=== SIMULARE INCEPUTA ===");

    logEvent("Simulare pornita. Max ticks: " + to_string(config->maxTicks));
    
    chrono::high_resolution_clock::time_point startTime = chrono::high_resolution_clock::now();
    
    while (currentTick < totalTicks) {
        currentTick++;
        
        if (currentTick % 100 == 0) {
          // cout << "Tick " << currentTick << "/" << totalTicks 
          //       << " | Pachete livrate: " << packagesDelivered 
          //       << " | Agenti activi: " << agentsAlive << endl;

            logEvent("--- HEARTBEAT spawnPackages();: Tick " + to_string(currentTick) + " ---"); // In fisier
        }
        
        spawnPackages();
        
        vector<Agent*> rawAgents;
        for (auto& agent : agents) {
            rawAgents.push_back(agent.get());
        }
        
        vector<Package*> rawPackages;
        for (auto& package : packages) {
            rawPackages.push_back(package.get()); 
        }
        
        hiveMind->update(rawAgents, rawPackages, *map, currentTick);
        
        updateAgents();
        
        processDeliveries();
        
        checkAgentStatus();
        
        if (agentsAlive == 0) {
            logEvent("Toti agentii au murit! Simularea se opreste prematur.");
            break;
        }
    }
    
    chrono::high_resolution_clock::time_point endTime = chrono::high_resolution_clock::now();
    chrono::milliseconds duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    
    for (auto& package : packages) {
        if (!package->delivered) {
            totalPenalties += 200;
            packagesFailed++;
        }
    }
    

    saveStatistics();
    
    logEvent("=== SIMULARE TERMINATA ===");
    logEvent("Durata: " + to_string(duration.count()) + " ms");
}

void Simulation::saveStatistics() {
    ofstream report("simulation_report.txt");
    
    if (!report.is_open()) {
        throw std::runtime_error("Eroare: Nu pot crea fisierul de raport!");
    }
    
    long long totalProfit = totalRevenue - totalCosts - totalPenalties;
    
    report << "=== RAPORT FINAL SIMULARE HIVEMIND ===\n\n";
    report << "SETARI:\n";
    report << "Ticks totali: " << totalTicks << "\n";
    report << "Ticks rulati: " << currentTick << "\n";
    report << "Dimensiune harta: " << map->getWidth() << "x" << map->getHeight() << "\n";
    report << "Agenti initiali: " << agents.size() << "\n";
    report << "Pachete generate: " << packages.size() << "\n\n";
    
    report << "STATISTICI OPERATIONALE:\n";
    report << "Agenti supravietuiti: " << agentsAlive << "\n";
    report << "Agenti pierduti: " << agentsLost << "\n";
    report << "Pachete livrate: " << packagesDelivered << "\n";
    report << "Pachete nelivrate: " << packagesFailed << "\n";
    report << "Rata de succes: " << fixed << setprecision(2) 
           << (packages.empty() ? 0.0 : (packagesDelivered * 100.0 / packages.size())) 
           << "%\n\n";
    report << "STATISTICI FINANCIARE:\n";
    report << "Profit Maxim: " << totalRevenue - totalCosts << " credite\n";
    report << "Venituri totale: " << totalRevenue << " credite\n";
    report << "Costuri totale: " << totalCosts << " credite\n";
    report << "Penalizari totale: " << totalPenalties << " credite\n";
    report << "  - Agent mort: " << (agentsLost * 500) << " credite\n";
    report << "  - Pachete intarziate: " << (totalPenalties - (agentsLost * 500) - (packagesFailed * 200)) 
           << " credite (50 per pachet)\n";
    report << "  - Pachete nelivrate: " << (packagesFailed * 200) << " credite (200 per pachet)\n";
    report << "PROFIT NET: " << totalProfit << " credite\n\n";
    
    report << "DETALII AGENTI:\n";
    int drones = 0, robots = 0, scooters = 0;
    int dronesAlive = 0, robotsAlive = 0, scootersAlive = 0;
    
    for (auto& agent : agents) {
        switch (agent->getType()) {
            case DRONE: drones++; if (agent->isAlive()) dronesAlive++; break;
            case ROBOT: robots++; if (agent->isAlive()) robotsAlive++; break;
            case SCOOTER: scooters++; if (agent->isAlive()) scootersAlive++; break;
        }
    }
    
    report << "Drone: " << dronesAlive << "/" << drones << " supravietuitoare\n";
    report << "Roboti: " << robotsAlive << "/" << robots << " supravietuitoare\n";
    report << "Scutere: " << scootersAlive << "/" << scooters << " supravietuitoare\n";
    
    report.close();
    
    logEvent("Raport salvat in simulation_report.txt");
}

void Simulation::printFinalReport() const {
   //NOT IMPLEMENTED
 }
