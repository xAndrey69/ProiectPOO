#include "hivemind.h"
#include "map.h"
#include "agents.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <limits>
#include <iostream>

using namespace std;

// Găsește cel mai apropiat punct de încărcare
Point HiveMind::findNearestChargingPoint(const Point& position, const Map& map) const {
    Point nearest = map.getBasePosition();
    int minDist = Point::distance(position, nearest);
    
    // Verifică toate stațiile
    for (const auto& station : map.getStations()) {
        int dist = Point::distance(position, station);
        if (dist < minDist) {
            minDist = dist;
            nearest = station;
        }
    }
    
    return nearest;
}

// Verifică dacă agentul are nevoie să se încarce înainte de misiune
bool HiveMind::needsCharging(const Agent* agent, const Point& destination, const Map& map) const {
    // Dacă e dronă și are destulă baterie, nu are nevoie să se încarce special
    if (agent->getType() == DRONE) {
        // Dronele au baterie mică - verifică dacă poate face drumul dus-întors
        int dist = Point::distance(agent->getPosition(), destination);
        float batteryNeeded = dist * agent->getConsumption() / agent->getSpeed();
        return agent->getBattery() < batteryNeeded * 1.5f; // Marjă de 50%
    }
    
    // Pentru roboți și scutere: verifică dacă poate ajunge la destinație și înapoi la o stație
    Point nearestCharger = findNearestChargingPoint(destination, map);
    
    int distToDest = Point::distance(agent->getPosition(), destination);
    int distToCharger = Point::distance(destination, nearestCharger);
    int totalDist = distToDest + distToCharger;
    
    float batteryNeeded = totalDist * agent->getConsumption() / agent->getSpeed();
    float safetyMargin = batteryNeeded * (params.safeBatteryMargin / 100.0f);
    
    return agent->getBattery() < (batteryNeeded + safetyMargin);
}

// Estimează timpul de livrare (în ticks)
int HiveMind::estimateDeliveryTime(const Agent* agent, const Point& destination) const {

    double distance;
    
    if(agent->getType() == DRONE){
	//calculam distanta euclidiana, ca zburam pe diagonala
	distance = std::hypot(destination.x - agent->getPosition().x, destination.y - agent->getPosition().y);
    } else {
	//manhattan
	distance = static_cast<double>(Point::distance(agent->getPosition(), destination));
    }

    //pentru ca nu am apelat iar bfs sa vedem exact distanta, vom folosi o euristica
    float pathFactor = (agent->getType() == DRONE) ? 1.0f : 1.3f;
    
    // Timp estimat = distanță * factor / viteză
    return static_cast<int>(ceil((distance * pathFactor) / agent->getSpeed()));
}

// Estimează costul livrării
double HiveMind::estimateDeliveryCost(const Agent* agent, int deliveryTime) const {
    // Cost = cost_per_tick * timp + consum * timp (consumul e deja în cost?)
    // În specificație, cost_per_tick include deja consumul
    return agent->getOperationalCost() * deliveryTime;
}

// Calculează scorul pentru o atribuire specifică
double HiveMind::calculateAssignmentScore(Agent* agent, Package* package,
                                         const Map& map, int currentTick) const {
    // Verifică dacă agentul poate livra pachetul
    if (agent->getBatteryPercentage() < params.criticalBatteryThreshold) {
        return -1000.0; // Agentul are baterie critică - nu îi da misiuni
    }
    
    // Estimează timpul și costul
    int deliveryTime = estimateDeliveryTime(agent, package->destCoord);
    double deliveryCost = estimateDeliveryCost(agent, deliveryTime);
    
    // Calculează profitul brut
    double grossProfit = package->reward - deliveryCost;
    
    // Penalizare pentru întârziere
    double delayPenalty = 0.0;
    int timeUntilDeadline = package->deadline - currentTick;
    
     if (deliveryTime > timeUntilDeadline) {
        delayPenalty = 50.0; // DOAR 50 de credite pentru întârziere
    }
    
    double netProfit = grossProfit - delayPenalty;
    
    // Factor de risc al bateriei (0 = sigur, 1 = riscant)
    float batteryRisk = 0.0f;
    float batteryNeeded = deliveryTime * agent->getConsumption();
    float batteryPercentageNeeded = (batteryNeeded / agent->getBattery()) * 100.0f;
    
    if (batteryPercentageNeeded > 80) batteryRisk = 1.0f;
    else if (batteryPercentageNeeded > 60) batteryRisk = 0.7f;
    else if (batteryPercentageNeeded > 40) batteryRisk = 0.4f;
    else if (batteryPercentageNeeded > 20) batteryRisk = 0.2f;
    
    // Factor de urgență
    double urgencyFactor = 1.0;
    if (timeUntilDeadline - deliveryTime < 3) {
        urgencyFactor = 2.0; // Foarte urgent
    } else if (timeUntilDeadline - deliveryTime < 8) {
        urgencyFactor = 1.5; // Urgent
    }
    
    // Distanța față de bază (preferă agenții apropriați)
    double distanceFactor = 1.0;
    int distToBase = Point::distance(agent->getPosition(), map.getBasePosition());
    if (distToBase > 10) {
        distanceFactor = 0.8; // Preferă agenții apropiați de bază
    }
    
    // Scorul final (ponderat)
    double score = 0.0;
    
    // Profitul (normalizat la 0-800)
    score += params.profitWeight * (netProfit / 800.0);
    
    // Siguranța bateriei (1 - risc)
    score += params.safetyWeight * (1.0 - batteryRisk);
    
    // Urgența
    score += params.urgencyWeight * (urgencyFactor / (deliveryTime + 1));
    
    // Distanța
    score += params.distanceWeight * distanceFactor;
    
    // Bonusuri specifice tipului de agent
    if (agent->getType() == ROBOT && package->reward < 400) {
        score += 0.2; // Roboții sunt buni pentru pachete ieftine
    } else if (agent->getType() == DRONE && package->reward > 600 && timeUntilDeadline < 15) {
        score += 0.3; // Dronele sunt bune pentru pachete scumpe și urgente
    } else if (agent->getType() == SCOOTER && deliveryTime >= 5 && deliveryTime <= 15) {
        score += 0.1; // Scuterele sunt bune pentru distanțe medii
    }
    
    return score;
}

// Gestionează agenții cu baterie scăzută
void HiveMind::handleLowBatteryAgents(vector<Agent*>& agents, const Map& map) {
    for (auto agent : agents) {
        if (!agent->isAlive() || agent->getState() == CHARGING) continue;
        
        float batteryPercent = agent->getBatteryPercentage();
        
        // Baterie critică - trimite imediat la încărcare
        if (batteryPercent < params.criticalBatteryThreshold) {
            Point charger = findNearestChargingPoint(agent->getPosition(), map);
            agent->sendToCharge(charger);
        }
        // Baterie scăzută și nu are misiune - se duce să se încarce preventiv
        else if (batteryPercent < params.lowBatteryThreshold && !agent->isBusy()) {
            Point charger = findNearestChargingPoint(agent->getPosition(), map);
            agent->sendToCharge(charger);
        }
    }
}

// Atribuie pachetele agenților
void HiveMind::assignPackages(vector<Agent*>& agents, vector<Package*>& packages,
                             const Map& map, int currentTick) {
    // Colectează toate scorurile posibile
    vector<AssignmentScore> allScores;
    
    for (auto agent : agents) {
        if (!agent->isAlive() || agent->isBusy()) continue;
        
        for (auto package : packages) {
            if (package->assigned || package->delivered) continue;
            
            // Verifică dacă agentul poate livra acest pachet
            double score = calculateAssignmentScore(agent, package, map, currentTick);
            
            if (score > 0) { // Ignoră atribuirile cu scor negativ
                allScores.emplace_back(agent, package, score, 
                                      package->reward - estimateDeliveryCost(agent, 
                                      estimateDeliveryTime(agent, package->destCoord)),
                                      estimateDeliveryTime(agent, package->destCoord),
                                      static_cast<int>((estimateDeliveryTime(agent, package->destCoord) * 
                                      agent->getConsumption() / agent->getBattery()) * 100));
            }
        }
    }
    
    // Sortează descrescător după scor
    sort(allScores.begin(), allScores.end(),
         [](const AssignmentScore& a, const AssignmentScore& b) {
             return a.score > b.score;
         });
    
    // Atribuie folosind algoritm greedy
    vector<bool> agentAssigned(agents.size(), false);
    vector<bool> packageAssigned(packages.size(), false);
    
    for (const auto& score : allScores) {
        // Găsește indexurile
        int agentIdx = -1;
        for (size_t i = 0; i < agents.size(); i++) {
            if (agents[i] == score.agent) {
                agentIdx = i;
                break;
            }
        }
        
        int packageIdx = -1;
        for (size_t i = 0; i < packages.size(); i++) {
            if (packages[i] == score.package) {
                packageIdx = i;
                break;
            }
        }
        
        if (agentIdx != -1 && packageIdx != -1 &&
            !agentAssigned[agentIdx] && !packageAssigned[packageIdx]) {
            
            // Verifică dacă agentul are nevoie să se încarce înainte
            if (needsCharging(score.agent, score.package->destCoord, map)) {
                Point charger = findNearestChargingPoint(score.agent->getPosition(), map);
                score.agent->sendToCharge(charger);
            } else {
                score.agent->assignTask(score.package, score.package->destCoord);
                score.package->assigned = true;
            }
            
            agentAssigned[agentIdx] = true;
            packageAssigned[packageIdx] = true;
        }
    }
}

// Optimizează agenții inactivi
void HiveMind::optimizeIdleAgents(vector<Agent*>& agents, const Map& map) {
    for (auto agent : agents) {
        if (!agent->isAlive() || agent->isBusy()) continue;
        
        // Dacă agentul este idle și nu are baterie plină, îl trimitem la încărcare
        if (agent->getState() == IDLE && agent->getBatteryPercentage() < 90.0f) {
            Point charger = findNearestChargingPoint(agent->getPosition(), map);
            if (agent->getPosition() != charger) {
                agent->sendToCharge(charger);
            }
        }
    }
}

// Metoda principală de update
void HiveMind::update(vector<Agent*>& agents, vector<Package*>& packages,
                     const Map& map, int currentTick) {
    
    // 1. Gestionează agenții cu baterie critică/scăzută
    handleLowBatteryAgents(agents, map);
    
    // 2. Atribuie pachetele
    assignPackages(agents, packages, map, currentTick);
    
    // 3. Optimizează agenții inactivi
    optimizeIdleAgents(agents, map);
}
