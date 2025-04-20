#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <ctime>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <cmath>

using namespace std;

// Forward declarations
struct Aircraft;
struct Runway;

// Constants and enums (same as before)
const int SIMULATION_DURATION = 300;
const int TIME_STEP = 1;
const double HOLDING_MIN_TIME = 120;
const double TAXI_MAX_TIME = 90;
const double GATE_TURNAROUND_TIME = 1800;

enum AircraftType { COMMERCIAL, CARGO, EMERGENCY };
enum FlightPhase { HOLDING, APPROACH, LANDING, TAXI, AT_GATE, TAKEOFF_ROLL, CLIMB, DEPARTURE_CRUISE, INVALID };
enum Direction { NORTH, SOUTH, EAST, WEST };
enum RunwayID { RWY_A, RWY_B, RWY_C };

// Now define Aircraft first
struct Aircraft {
    string flightNumber;
    struct Airline* airline;  // Forward declaration
    AircraftType type;
    Direction direction;
    bool isEmergency;
    bool hasAVN;
    FlightPhase phase;
    double speed;
    double targetSpeed;
    bool isFaulty;
    double phaseStartTime;
};

// Then define Airline which uses Aircraft
struct Airline {
    string name;
    AircraftType type;
    int aircraftCount;
    int flightCount;
    int violations;
};

// Finally define Runway which uses Aircraft
struct Runway {
    RunwayID id;
    string name;
    bool isOccupied;
    Aircraft* currentAircraft;
};

// Global variables
mutex coutMutex;
double simTime = 0;

// Helper functions
string generateFlightNumber(const Airline& airline, int sequence) {
    string prefix;
    if (airline.name == "PIA") prefix = "PK";
    else if (airline.name == "AirBlue") prefix = "PA";
    else if (airline.name == "FedEx") prefix = "FX";
    else if (airline.name == "Pakistan Airforce") prefix = "PF";
    else if (airline.name == "Blue Dart") prefix = "BD";
    else if (airline.name == "AghaKhan Air Ambulance") prefix = "AK";
    
    return prefix + to_string(100 + sequence);
}

string getPhaseName(FlightPhase phase) {
    switch (phase) {
        case HOLDING: return "Holding";
        case APPROACH: return "Approach";
        case LANDING: return "Landing";
        case TAXI: return "Taxi";
        case AT_GATE: return "At Gate";
        case TAKEOFF_ROLL: return "Takeoff Roll";
        case CLIMB: return "Climb";
        case DEPARTURE_CRUISE: return "Departure Cruise";
        default: return "Invalid";
    }
}

string getDirectionName(Direction dir) {
    switch (dir) {
        case NORTH: return "North";
        case SOUTH: return "South";
        case EAST: return "East";
        case WEST: return "West";
        default: return "Unknown";
    }
}

string getAircraftTypeName(AircraftType type) {
    switch (type) {
        case COMMERCIAL: return "Commercial";
        case CARGO: return "Cargo";
        case EMERGENCY: return "Emergency";
        default: return "Unknown";
    }
}

void initializeAirlines(vector<Airline>& airlines) {
    airlines = {
        {"PIA", COMMERCIAL, 6, 4, 0},
        {"AirBlue", COMMERCIAL, 4, 4, 0},
        {"FedEx", CARGO, 3, 2, 0},
        {"Pakistan Airforce", EMERGENCY, 2, 1, 0},
        {"Blue Dart", CARGO, 2, 2, 0},
        {"AghaKhan Air Ambulance", EMERGENCY, 2, 1, 0}
    };
}

void initializeRunways(vector<Runway>& runways) {
    runways.clear();  // Clear existing elements
    runways.push_back({RWY_A, "RWY-A (North-South Arrivals)", false, nullptr});
    runways.push_back({RWY_B, "RWY-B (East-West Departures)", false, nullptr});
    runways.push_back({RWY_C, "RWY-C (Cargo/Emergency/Overflow)", false, nullptr});
}

void updateSpeedSmoothly(Aircraft& aircraft) {
    const double acceleration = 1.5; // km/h per second
    const double deceleration = 2.0; // km/h per second
    
    if (fabs(aircraft.speed - aircraft.targetSpeed) < 0.1) {
        aircraft.speed = aircraft.targetSpeed;
        return;
    }

    if (aircraft.speed < aircraft.targetSpeed) {
        aircraft.speed = min(aircraft.targetSpeed, 
                           aircraft.speed + acceleration);
    } else {
        aircraft.speed = max(aircraft.targetSpeed, 
                           aircraft.speed - deceleration);
    }
}

Runway* assignRunway(Aircraft& aircraft, vector<Runway>& runways) {
    // Emergency and cargo prefer RWY-C
    if (aircraft.isEmergency || aircraft.type == EMERGENCY || aircraft.type == CARGO) {
        if (!runways[RWY_C].isOccupied) return &runways[RWY_C];
    }
    
    // Normal runway assignment
    if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
        if (!runways[RWY_A].isOccupied) return &runways[RWY_A];
    } else {
        if (!runways[RWY_B].isOccupied) return &runways[RWY_B];
    }
    
    // Fallback to RWY-C if available
    if (!runways[RWY_C].isOccupied) return &runways[RWY_C];
    
    return nullptr;
}

void checkForViolations(Aircraft& aircraft) {
    bool violation = false;
    
    switch (aircraft.phase) {
        case HOLDING:
            violation = (aircraft.speed > 600);
            break;
        case APPROACH:
            violation = (aircraft.speed < 240 || aircraft.speed > 290);
            break;
        case LANDING:
            violation = (aircraft.speed > 240);
            break;
        case TAXI:
            violation = (aircraft.speed > 30);
            break;
        case AT_GATE:
            violation = (aircraft.speed > 10);
            break;
        case TAKEOFF_ROLL:
            violation = (aircraft.speed > 290);
            break;
        case CLIMB:
            violation = (aircraft.speed > 463);
            break;
        case DEPARTURE_CRUISE:
            violation = (aircraft.speed < 800 || aircraft.speed > 900);
            break;
        default:
            break;
    }
    
    if (violation && !aircraft.hasAVN) {
        aircraft.hasAVN = true;
        aircraft.airline->violations++;
        
        lock_guard<mutex> guard(coutMutex);
        cout << "VIOLATION: " << aircraft.flightNumber << " (" << aircraft.airline->name 
             << ") exceeded speed limits in " << getPhaseName(aircraft.phase) 
             << " phase (Speed: " << aircraft.speed << " km/h)" << endl;
    }
}

void simulateArrival(Aircraft& aircraft, vector<Runway>& runways) {
    double phaseDuration = simTime - aircraft.phaseStartTime;

    switch (aircraft.phase) {
        case HOLDING:
            aircraft.targetSpeed = 450 + rand() % 151; // 450-600 km/h
            if (phaseDuration >= HOLDING_MIN_TIME && (rand() % 30 == 0)) {
                aircraft.phase = APPROACH;
                aircraft.phaseStartTime = simTime;
                aircraft.targetSpeed = 265;
            }
            break;

        case APPROACH: {
            Runway* runway = assignRunway(aircraft, runways);
            if (runway) {
                aircraft.phase = LANDING;
                aircraft.phaseStartTime = simTime;
                aircraft.targetSpeed = 240;
                runway->isOccupied = true;
                runway->currentAircraft = &aircraft;
            }
            break;
        }

        case LANDING:
            if (phaseDuration > 5.0) {
                aircraft.targetSpeed = 30;
            }
            if (aircraft.speed <= 35.0 && phaseDuration > 10.0) {
                aircraft.phase = TAXI;
                aircraft.phaseStartTime = simTime;
                aircraft.targetSpeed = 20;
                // Free runway
                for (auto& rwy : runways) {
                    if (rwy.currentAircraft == &aircraft) {
                        rwy.isOccupied = false;
                        rwy.currentAircraft = nullptr;
                        break;
                    }
                }
            }
            break;

        case TAXI:
            if (phaseDuration >= TAXI_MAX_TIME || (phaseDuration > 30.0 && rand() % 10 == 0)) {
                aircraft.phase = AT_GATE;
                aircraft.phaseStartTime = simTime;
                aircraft.targetSpeed = 0;
            }
            break;

        case AT_GATE:
            if (phaseDuration >= GATE_TURNAROUND_TIME) {
                // Convert to departure
                aircraft.direction = (rand() % 2 == 0) ? EAST : WEST;
                aircraft.phase = AT_GATE;
                aircraft.phaseStartTime = simTime;
            }
            break;

        default: break;
    }
    updateSpeedSmoothly(aircraft);
    checkForViolations(aircraft);
}

void simulateDeparture(Aircraft& aircraft, vector<Runway>& runways) {
    double phaseDuration = simTime - aircraft.phaseStartTime;

    switch (aircraft.phase) {
        case AT_GATE:
            if (phaseDuration >= 120.0) {
                aircraft.phase = TAXI;
                aircraft.phaseStartTime = simTime;
                aircraft.targetSpeed = 20;
            }
            break;

        case TAXI: {
            Runway* runway = assignRunway(aircraft, runways);
            if (runway) {
                aircraft.phase = TAKEOFF_ROLL;
                aircraft.phaseStartTime = simTime;
                aircraft.targetSpeed = 0;
                runway->isOccupied = true;
                runway->currentAircraft = &aircraft;
            }
            break;
        }

        case TAKEOFF_ROLL:
            if (phaseDuration < 5.0) {
                aircraft.targetSpeed = 80;
            } else if (phaseDuration < 15.0) {
                aircraft.targetSpeed = 180;
            } else {
                aircraft.targetSpeed = 290;
            }
            if (aircraft.speed >= 280.0 && phaseDuration > 20.0) {
                aircraft.phase = CLIMB;
                aircraft.phaseStartTime = simTime;
                aircraft.targetSpeed = 350;
                // Free runway
                for (auto& rwy : runways) {
                    if (rwy.currentAircraft == &aircraft) {
                        rwy.isOccupied = false;
                        rwy.currentAircraft = nullptr;
                        break;
                    }
                }
            }
            break;

        case CLIMB:
            if (phaseDuration > 30.0) {
                aircraft.targetSpeed = 450;
            }
            if (phaseDuration > 60.0) {
                aircraft.phase = DEPARTURE_CRUISE;
                aircraft.phaseStartTime = simTime;
                aircraft.targetSpeed = 850;
            }
            break;

        case DEPARTURE_CRUISE:
            if (phaseDuration > 120.0) {
                aircraft.phase = INVALID;
            }
            break;

        default: break;
    }
    updateSpeedSmoothly(aircraft);
    checkForViolations(aircraft);
}

void handleGroundFaults(vector<Aircraft>& aircrafts) {
    for (auto& aircraft : aircrafts) {
        if ((aircraft.phase == TAXI || aircraft.phase == AT_GATE) && !aircraft.isFaulty) {
            if (rand() % 100 < 5) {
                aircraft.isFaulty = true;
                
                lock_guard<mutex> guard(coutMutex);
                cout << "FAULT DETECTED: " << aircraft.flightNumber << " (" 
                     << aircraft.airline->name << ") has a ground fault and is being towed." << endl;
            }
        }
    }
}

void displayStatus(const vector<Aircraft>& aircrafts, const vector<Runway>& runways) {
    lock_guard<mutex> guard(coutMutex);
    
    int minutes = static_cast<int>(simTime) / 60;
    int seconds = static_cast<int>(simTime) % 60;
    
    cout << "\n\n=== AirControlX Status at " << setw(2) << setfill('0') << minutes 
         << ":" << setw(2) << setfill('0') << seconds << " ===" << endl;
    
    cout << "\nRunway Status:" << endl;
    for (const auto& runway : runways) {
        cout << runway.name << ": ";
        if (runway.isOccupied && runway.currentAircraft != nullptr) {
            cout << "Occupied by " << runway.currentAircraft->flightNumber 
                 << " (" << runway.currentAircraft->airline->name << ")";
        } else {
            cout << "Available";
        }
        cout << endl;
    }
    
    cout << "\nActive Aircraft (" << aircrafts.size() << "):" << endl;
    for (const auto& aircraft : aircrafts) {
        if (aircraft.isFaulty) continue;
        
        cout << aircraft.flightNumber << " (" << aircraft.airline->name << "): "
             << getDirectionName(aircraft.direction) << " " 
             << (aircraft.direction == NORTH || aircraft.direction == SOUTH ? "Arrival" : "Departure")
             << ", " << getAircraftTypeName(aircraft.type)
             << (aircraft.isEmergency ? " [EMERGENCY]" : "")
             << ", Phase: " << getPhaseName(aircraft.phase)
             << ", Speed: " << aircraft.speed << " km/h"
             << (aircraft.hasAVN ? " [AVN ISSUED]" : "") << endl;
    }
    
    cout << "==================================" << endl;
}

Aircraft createAircraft(const vector<Airline>& airlines, Direction direction, int seq) {
    bool isEmergency = false;
    int emergencyProb = 0;
    
    switch (direction) {
        case NORTH: emergencyProb = 10; break;
        case SOUTH: emergencyProb = 5; break;
        case EAST: emergencyProb = 15; break;
        case WEST: emergencyProb = 20; break;
    }
    
    isEmergency = (rand() % 100) < emergencyProb;
    
    vector<const Airline*> eligibleAirlines;
    for (const auto& airline : airlines) {
        if (isEmergency) {
            if (airline.type == EMERGENCY) {
                eligibleAirlines.push_back(&airline);
            }
        } else {
            if (airline.type != EMERGENCY) {
                eligibleAirlines.push_back(&airline);
            }
        }
    }
    
    if (eligibleAirlines.empty()) {
        eligibleAirlines.push_back(&airlines[0]);
    }
    
    const Airline* selectedAirline = eligibleAirlines[rand() % eligibleAirlines.size()];
    
    Aircraft aircraft;
    aircraft.flightNumber = generateFlightNumber(*selectedAirline, seq);
    aircraft.airline = const_cast<Airline*>(selectedAirline);
    aircraft.type = selectedAirline->type;
    aircraft.direction = direction;
    aircraft.isEmergency = isEmergency;
    aircraft.hasAVN = false;
    aircraft.isFaulty = false;
    aircraft.phaseStartTime = simTime;
    
    if (direction == NORTH || direction == SOUTH) {
        aircraft.phase = HOLDING;
        aircraft.speed = 400 + rand() % 201;
        aircraft.targetSpeed = 450 + rand() % 151;
    } else {
        aircraft.phase = AT_GATE;
        aircraft.speed = 0;
        aircraft.targetSpeed = 0;
    }
    
    return aircraft;
}

int main() {
    srand(time(0));
    vector<Airline> airlines;
    initializeAirlines(airlines);
    vector<Runway> runways;
    initializeRunways(runways);
    vector<Aircraft> activeAircrafts;

    cout << "AirControlX - Automated Air Traffic Control System" << endl;
    cout << "Simulation started for " << SIMULATION_DURATION << " seconds (" 
         << SIMULATION_DURATION/60 << " minutes)" << endl;

    auto startTime = chrono::steady_clock::now();
    int aircraftSequence = 0;

    while (simTime < SIMULATION_DURATION) {
        // Generate new flights
        if (static_cast<int>(simTime) % 180 == 0) { // North every 3 min
            activeAircrafts.push_back(createAircraft(airlines, NORTH, ++aircraftSequence));
        }
        if (static_cast<int>(simTime) % 120 == 0) { // South every 2 min
            activeAircrafts.push_back(createAircraft(airlines, SOUTH, ++aircraftSequence));
        }
        if (static_cast<int>(simTime) % 150 == 0) { // East every 2.5 min
            activeAircrafts.push_back(createAircraft(airlines, EAST, ++aircraftSequence));
        }
        if (static_cast<int>(simTime) % 240 == 0) { // West every 4 min
            activeAircrafts.push_back(createAircraft(airlines, WEST, ++aircraftSequence));
        }

        // Process each aircraft
        for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) {
            Aircraft& aircraft = *it;
            
            if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
                simulateArrival(aircraft, runways);
            } else {
                simulateDeparture(aircraft, runways);
            }

            if (aircraft.phase == INVALID) {
                it = activeAircrafts.erase(it);
            } else {
                ++it;
            }
        }

        // Periodic operations
        if (static_cast<int>(simTime) % 30 == 0) {
            handleGroundFaults(activeAircrafts);
        }

        // Display status
        displayStatus(activeAircrafts, runways);

        // Update time and sleep
        simTime = chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - startTime).count();
        this_thread::sleep_for(chrono::milliseconds(TIME_STEP * 1000));
    }

    // Final statistics
    cout << "\nSimulation completed. Final statistics:" << endl;
    for (const auto& airline : airlines) {
        cout << airline.name << " violations: " << airline.violations << endl;
    }

    return 0;
}
