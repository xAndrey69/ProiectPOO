#ifndef AGENTS_H
#define AGENTS_H

#include "utils.h"
#include <memory>

class Map;
struct Package;

enum AgentState { 
    IDLE,       
    MOVING,     
    CHARGING,   
    DEAD        
};

enum AgentType { 
    DRONE, 
    ROBOT, 
    SCOOTER 
};

class Agent {
protected:
    int id;
    AgentType type;
    Point position;
    Point target;
    float battery;
    float maxBattery;
    float consumption;
    int costPerTick;
    AgentState state;
    Package* currentPackage;
    bool hasPhysicalPackage;

public:
    Agent(int _id, int x, int y, AgentType _type, 
    float _maxBattery, float _consumption, int _costPerTick);
    virtual ~Agent() = default;

    virtual void move(const Map& map) = 0;
    virtual float getSpeed() const = 0;
    
    void charge();
    void assignTask(Package* pkg, Point dest);
    void sendToCharge(Point station);
    void dropPackage();
    void updatePosition(Point newPos);
    
    int getId() const { return id; }
    AgentType getType() const { return type; }
    AgentState getState() const { return state; }
    Point getPosition() const { return position; }
    Point getTarget() const { return target; }
    float getBattery() const { return battery; }
    float getBatteryPercentage() const { return (battery / maxBattery) * 100.0f; }
    float getConsumption() const { return consumption; }
    int getOperationalCost() const { return costPerTick; }
    bool isAlive() const { return state != DEAD; }
    bool isBusy() const { return currentPackage != nullptr; }
    Package* getPackage() const { return currentPackage; }
    
    void setState(AgentState newState) { state = newState; }
};

class Drone : public Agent {
public:
    Drone(int id, int x, int y);
    void move(const Map& map) override;
    float getSpeed() const override { return 3.0f; }
};

class Robot : public Agent {
public:
    Robot(int id, int x, int y);
    void move(const Map& map) override;
    float getSpeed() const override { return 1.0f; }
};

class Scooter : public Agent {
public:
    Scooter(int id, int x, int y);
    void move(const Map& map) override;
    float getSpeed() const override { return 2.0f; }
};

class AgentFactory {
public:
    static std::unique_ptr<Agent> create(AgentType type, int id, int x, int y);
};
#endif
