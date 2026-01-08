#include "simulation.h"
#include <iostream>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip> // Pentru setprecision și fixed
#include <memory>  // Pentru std::unique_ptr și std::make_unique

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
    
    // Generează harta
    mapGenerator->generate(*map);
    totalTicks = config->maxTicks;
    
    // Generează agenții inițiali
    generateInitialAgents();
    
    // Afișează informații inițiale
    // cout << "=== HIVEMIND SIMULATOR ===\n";
    // cout << "Harta: " << config->mapWidth << "x" << config->mapHeight << "\n";
    // cout << "Durata: " << totalTicks << " ticks\n";
    // cout << "Agenti: " << agents.size() << " (D:" << config->dronesCount 
    //      << " R:" << config->robotsCount << " S:" << config->scootersCount << ")\n";
    // cout << "Pachete totale: " << config->totalPackages << "\n";
    // cout << "Frecventa spawn: la fiecare " << config->spawnFrequency << " ticks\n";
    // cout << "==========================\n\n";
    
    logEvent("Simularea este gata să înceapă.");
}

void Simulation::generateInitialAgents() {
    Config* config = Config::getInstance();
    Point basePos = map->getBasePosition();
    
    int agentId = 0;
    
    // Creează drone
    for (int i = 0; i < config->dronesCount; i++) {
        agents.push_back(std::unique_ptr<Agent>(AgentFactory::create(DRONE, agentId++, basePos.x, basePos.y)));
    }
    
    // Creează roboți
    for (int i = 0; i < config->robotsCount; i++) {
        agents.push_back(std::unique_ptr<Agent>(AgentFactory::create(ROBOT, agentId++, basePos.x, basePos.y)));
    }
    
    // Creează scutere
    for (int i = 0; i < config->scootersCount; i++) {
        agents.push_back(std::unique_ptr<Agent>(AgentFactory::create(SCOOTER, agentId++, basePos.x, basePos.y)));
    }
    
    agentsAlive = agents.size();
    logEvent("Creati " + to_string(agents.size()) + " agenti initiali.");
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
    
    // Creează noul pachet cu unique_ptr
    auto newPackage = std::unique_ptr<Package>(new Package(
        (int)packages.size(),
        mapClients[clientIdx],
        rewardDist(gen),
        currentTick + deadlineDist(gen),
        currentTick,
        clientIdx
    ));
    
    // Adaugă pachetul la vector
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
        
        // Verifică dacă e parcat
        bool onChargingCell = (cell == CELL_BASE || cell == CELL_STATION);

        //debug pt drone
	if (agent->getType() == DRONE) {
            double distToTarget = 0.0;
            string statusTarget = "Idle";

            // Calculam distanta daca are un pachet (targetul e destinatia pachetului)
            if (agent->getPackage()) {
                Point dest = agent->getPackage()->destCoord;
                // Distanta Euclidiana
                distToTarget = std::hypot(dest.x - pos.x, dest.y - pos.y);
                statusTarget = "Livrare Pachet " + to_string(agent->getPackage()->id);
            } 
            // Optional: Daca nu are pachet dar se misca spre statie, ai putea calcula distanta pana acolo
            // dar momentan ne concentram pe pachet.

            // Construim mesajul de telemetrie
            // Format: [DRONE ID] Bat: 85.5% | Pos: (10, 20) | Target: 45.2m
            
            // Nota: Logarea la fiecare tick va genera un fisier foarte mare!
            // Folosim string stream pentru formatare frumoasa a float-urilor
            std::stringstream ss;
            ss << "[TELEMETRIE DRONA " << agent->getId() << "] "
               << "Bat: " << std::fixed << std::setprecision(1) << agent->getBatteryPercentage() << "% | "
               << "Pos: (" << pos.x << "," << pos.y << ") | "
               << "Status: " << statusTarget;
            
            if (agent->getPackage()) {
                ss << " | Dist. Dest: " << std::fixed << std::setprecision(2) << distToTarget;
            }

            logEvent(ss.str());
        }
	
        // 1. CALCUL COSTURI
        // Taxăm doar dacă se mișcă (pleacă/vine) sau stă degeaba în câmp
        // NU taxăm dacă e pe stație și stă cuminte (IDLE/CHARGING)
        if (!onChargingCell || agent->getState() == MOVING) {
            totalCosts += agent->getOperationalCost();
        }
        
        // 2. LOGICA DE STARE
        // Dacă e pe stație și NU vrea să plece (nu e MOVING), atunci încarcă
        if (onChargingCell && agent->getState() != MOVING) {
            
            if (agent->getBatteryPercentage() < 100.0f) {
                // --- FIX CRITIC AICI ---
                // Trebuie să setăm explicit starea, altfel funcția charge() s-ar putea să nu facă nimic

		if (agent->getType() == DRONE && agent->getState() != CHARGING) {
                    std::stringstream ssCharge;
                    ssCharge << "[INFO] Drona " << agent->getId() 
                             << " a ajuns la statie si INCEPE INCARCAREA. (Baterie curenta: " 
                             << std::fixed << std::setprecision(1) << agent->getBatteryPercentage() << "%)";
                    logEvent(ssCharge.str());
                }
		
                agent->setState(CHARGING); 
                agent->charge();
                
            } else {
                // E plin, îl trecem pe IDLE ca să fie pregătit de plecare
		if (agent->getType() == DRONE && agent->getState() == CHARGING) {
                     logEvent("[INFO] Drona " + to_string(agent->getId()) + " a TERMINAT incarcarea (100%).");
                }
                agent->setState(IDLE);
            }
            
        } else {
            // --- MIȘCARE ---
            float batteryBefore = agent->getBattery();
            agent->move(*map);
            
            // Verificare deces
            if (!agent->isAlive() && batteryBefore > 0) {

		string typeStr;
                switch(agent->getType()) {
                    case DRONE: typeStr = "DRONE"; break;
                    case ROBOT: typeStr = "ROBOT"; break;
                    case SCOOTER: typeStr = "SCOOTER"; break;
                    default: typeStr = "NECUNOSCUT"; break;
                }

                // Presupunând că ai o metodă getId() în clasa Agent. 
                // Dacă nu ai getId(), poți folosi indexul din buclă sau doar tipul.
                Point deathPos = agent->getPosition();
                logEvent("!!! DECES AGENT !!! ID: " + to_string(agent->getId()) + 
                         " [" + typeStr + "] a murit la coordonatele (" + 
                         to_string(deathPos.x) + ", " + to_string(deathPos.y) + 
                         "). Baterie epuizata.");
			 
                agentsLost++;
                agentsAlive--;
                totalPenalties += 500;
                if (agent->isBusy()) {
                    // Resetare pachet
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
    for (auto& agent : agents) { // Folosim referință
        if (!agent->isAlive() || !agent->isBusy()) continue;
        
        Package* package = agent->getPackage();
        if (!package) continue;
        
        // Verifică dacă agentul a ajuns la destinație
        if (agent->getPosition() == package->destCoord) {
            // Marchează ca livrat
            package->delivered = true;
            packagesDelivered++;
            totalRevenue += package->reward;

	    string deliveryMsg = "Pachet " + to_string(package->id) + 
                                 " RECEPTIONAT de client. Livrat de Agent " + 
                                 to_string(agent->getId()) + " [" + 
                                 (agent->getType() == DRONE ? "DRONA" : 
                                  (agent->getType() == ROBOT ? "ROBOT" : "SCUTER")) + "]";
            
            // Verifică întârzierea
            if (currentTick > package->deadline) {
                // 50 credite penalizare pentru întârziere
                totalPenalties += 50;
		int delay = currentTick - package->deadline;
                logEvent(deliveryMsg + " cu intarziere (" + to_string(delay) + 
                        " ticks). Penalizare: 50 credite");
            } else {
                logEvent(deliveryMsg + " la timp.");
            }
            
            // Eliberează agentul
            agent->dropPackage();
        }
    }
}

void Simulation::checkAgentStatus() {
    agentsAlive = 0;
    for (auto& agent : agents) { // Folosim referință
        if (agent->isAlive()) {
            agentsAlive++;
        }
    }
}

void Simulation::run() {
    Config* config = Config::getInstance();
    
   // cout << "Simularea a inceput...\n";
    logEvent("=== SIMULARE INCEPUTA ===");

    logEvent("Simulare pornita. Max ticks: " + to_string(config->maxTicks));
    
    chrono::high_resolution_clock::time_point startTime = chrono::high_resolution_clock::now();
    
    while (currentTick < totalTicks) {
        currentTick++;
        
        if (currentTick % 100 == 0) {
           // cout << "Tick " << currentTick << "/" << totalTicks 
          //       << " | Pachete livrate: " << packagesDelivered 
          //       << " | Agenti activi: " << agentsAlive << endl;

            logEvent("--- HEARTBEAT: Tick " + to_string(currentTick) + " ---"); // In fisier
        }
        
        // Pasul 1: Generează pachete noi
        spawnPackages();
        
        // Pasul 2: Actualizează HiveMind (algoritmul de decizie)
        // Vector de pointeri brute pentru HiveMind
        vector<Agent*> rawAgents;
        for (auto& agent : agents) {
            rawAgents.push_back(agent.get()); // .get() pentru pointer brut
        }
        
        vector<Package*> rawPackages;
        for (auto& package : packages) {
            rawPackages.push_back(package.get()); // .get() pentru pointer brut
        }
        
        hiveMind->update(rawAgents, rawPackages, *map, currentTick);
        
        // Pasul 3: Actualizează agenții (mișcare, consum, etc.)
        updateAgents();
        
        // Pasul 4: Procesează livrările
        processDeliveries();
        
        // Pasul 5: Verifică statusul agenților
        checkAgentStatus();
        
        // Dacă toți agenții au murit, oprește simularea prematur
        if (agentsAlive == 0) {
            logEvent("Toti agentii au murit! Simularea se opreste prematur.");
            break;
        }
    }
    
    chrono::high_resolution_clock::time_point endTime = chrono::high_resolution_clock::now();
    chrono::milliseconds duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    
    // Calculează penalizările finale pentru pachetele nelivrate
    for (auto& package : packages) {
        if (!package->delivered) {
            totalPenalties += 200; // 200 credite pentru nelivrare
            packagesFailed++;
        }
    }
    
    // Salvează statisticile finale
    if (enableLogging) {
        saveStatistics();
    }
    
    // Afișează raportul final
    printFinalReport();
    
  //  cout << "\nSimularea a durat " << duration.count() << " ms\n";
    logEvent("=== SIMULARE TERMINATA ===");
    logEvent("Durata: " + to_string(duration.count()) + " ms");
}

void Simulation::saveStatistics() {
    ofstream report("simulation_report.txt");
    
    if (!report.is_open()) {
        cerr << "Eroare: Nu pot crea fisierul de raport!\n";
        return;
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
//    long long totalProfit = totalRevenue - totalCosts - totalPenalties;
    
//     cout << "\n========================================\n";
//     cout << "          RAPORT FINAL SIMULARE\n";
//     cout << "========================================\n";
//     cout << "PERFORMANTA OPERATIONALA:\n";
//     cout << "  Agenti supravietuiti: " << agentsAlive << "/" << agents.size() 
//          << " (" << fixed << setprecision(1) 
//          << (agents.size() ? (agentsAlive * 100.0 / agents.size()) : 0) << "%)\n";
//     cout << "  Pachete livrate: " << packagesDelivered << "/" << packages.size()
//          << " (" << fixed << setprecision(1) 
//          << (packages.empty() ? 0.0 : (packagesDelivered * 100.0 / packages.size())) 
//          << "%)\n\n";
    
//     cout << "REZULTATE FINANCIARE:\n";
//     cout << "  Venituri: " << totalRevenue << " credite\n";
//     cout << "  Costuri:  " << totalCosts << " credite\n";
//     cout << "  Penalizari: " << totalPenalties << " credite\n";
//     cout << "  --------------------\n";
//     cout << "  PROFIT NET: " << totalProfit << " credite\n\n";
    
//     if (totalProfit > 0) {
//         cout << "  REZULTAT: PROFITABIL! ✓\n";
//     } else {
//         cout << "  REZULTAT: PIERDERE! ✗\n";
//     }
    
//     cout << "========================================\n";
 }
