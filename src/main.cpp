#include "config.h"
#include "simulation.h"
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>

// BENCHMARK
const int TOTAL_ITERATIONS = 100000; 

std::mutex resultsMutex;
long long globalProfit = 0;
long long globalSurvivors = 0;
long long globalDelivered = 0;

std::atomic<int> progressCounter(0);

void workerThread(int iterationsToRun) {
    long long localProfit = 0;
    long long localSurvivors = 0;
    long long localDelivered = 0;

    for (int i = 0; i < iterationsToRun; ++i) {
        try {
            Simulation sim(false); 
            sim.initialize();
            sim.run();

            localProfit += sim.getTotalProfit();
            localSurvivors += sim.getAgentsAlive(); 
            localDelivered += sim.getPackagesDelivered();

        } catch (const std::exception& e) {
            
        }
        
        progressCounter++;
    }

    std::lock_guard<std::mutex> lock(resultsMutex);
    globalProfit += localProfit;
    globalSurvivors += localSurvivors;
    globalDelivered += localDelivered;
}

void runBenchmark() {
    Config* config = Config::getInstance();
    config->loadFromFile("../simulation_setup.txt");

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    
    std::cout << "--- BENCHMARK MULTI-THREADED ---" << std::endl;
    std::cout << "Sistem: " << numThreads << " nuclee CPU detectate." << std::endl;
    std::cout << "Task: " << TOTAL_ITERATIONS << " simulari." << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    int iterationsPerThread = TOTAL_ITERATIONS / numThreads;
    int remainder = TOTAL_ITERATIONS % numThreads;

    for (unsigned int i = 0; i < numThreads; ++i) {
        int count = iterationsPerThread + (i == numThreads - 1 ? remainder : 0);
        
        threads.emplace_back(workerThread, count);
    }

    while (progressCounter < TOTAL_ITERATIONS) {
        int current = progressCounter.load();
        int percent = (current * 100) / TOTAL_ITERATIONS;
        
        std::cout << "\rProgres: [" << percent << "%] " << current << "/" << TOTAL_ITERATIONS << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (current >= TOTAL_ITERATIONS) break;
    }
    std::cout << "\rProgres: [100%] " << TOTAL_ITERATIONS << "/" << TOTAL_ITERATIONS << " Done!" << std::endl;

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;

    std::cout << "\n========================================" << std::endl;
    std::cout << "REZULTATE FINALE (" << numThreads << " Threads)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Timp Executie:       " << std::fixed << std::setprecision(2) << elapsed.count() << " secunde" << std::endl;
    std::cout << "Viteza:              " << (int)(TOTAL_ITERATIONS / elapsed.count()) << " simulari/sec" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "PROFIT MEDIU:        " << (double)globalProfit / TOTAL_ITERATIONS << std::endl;
    std::cout << "SURVIVABILITY AVG:   " << (double)globalSurvivors / TOTAL_ITERATIONS << std::endl;
    std::cout << "PACHETE LIVRATE AVG: " << (double)globalDelivered / TOTAL_ITERATIONS << std::endl;
    std::cout << "========================================" << std::endl;
}

void runNormal() {
    Config* config = Config::getInstance();
    config->loadFromFile("../simulation_setup.txt");
    Simulation sim(true); 
    sim.initialize();
    sim.run();
    sim.printFinalReport();
}

int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--benchmark") {
            runBenchmark();
        } else {
            runNormal();
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Eroare: " << e.what() << std::endl;
        return 1;
    }
}
