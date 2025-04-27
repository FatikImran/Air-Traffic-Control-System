/*

---------   MUHAMMAD FATIK BIN IMRAN (23i-0655)
---------   MUHAMMAD KALEEM AKHTAR (23i-0524)
-----   SECTION: BCS-4C

*/

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

using namespace std;

// Constants
const int SIMULATION_DURATION = 300; 		// 5 minutes in seconds
const int TIME_STEP = 1; 			// 1 second per simulation step

// Enums
enum AircraftType { COMMERCIAL, CARGO, EMERGENCY };
enum FlightPhase { 
    HOLDING, APPROACH, LANDING, TAXI, AT_GATE, 	// Arrival phases
    TAKEOFF_ROLL, CLIMB, DEPARTURE_CRUISE,      // Departure phases
    INVALID
};
enum Direction { NORTH, SOUTH, EAST, WEST };
enum RunwayID { RWY_A, RWY_B, RWY_C };

// Airline structure
struct Airline {
    string name;
    AircraftType type;
    int aircraftCount;
    int flightCount;
    int violations;
};

// Aircraft structure
struct Aircraft {
    string flightNumber;
    Airline* airline;
    AircraftType type;
    Direction direction;
    bool isEmergency;
    bool hasAVN;
    FlightPhase phase;
    double speed; // km/h
    bool isFaulty;
};

// Runway structure
struct Runway {
    RunwayID id;
    string name;
    bool isOccupied;
    Aircraft* currentAircraft;
};

// Function prototypes
void initializeAirlines(vector<Airline>& airlines);
void initializeRunways(vector<Runway>& runways);
string generateFlightNumber(const Airline& airline, int sequence);
Aircraft createAircraft(const vector<Airline>& airlines, Direction direction, int seq);
void simulateArrival(Aircraft& aircraft, vector<Runway>& runways);
void simulateDeparture(Aircraft& aircraft, vector<Runway>& runways);
void updateAircraftSpeed(Aircraft& aircraft);
void checkForViolations(Aircraft& aircraft);
void displayStatus(const vector<Aircraft>& aircrafts, const vector<Runway>& runways, int elapsedTime);
void handleGroundFaults(vector<Aircraft>& aircrafts);
Runway* assignRunway(Aircraft& aircraft, vector<Runway>& runways);
string getPhaseName(FlightPhase phase);
string getDirectionName(Direction dir);
string getAircraftTypeName(AircraftType type);

// Global mutex for thread-safe console output
mutex coutMutex;
bool cargoCreated = false;

int main() {
    srand(time(0));

    // Initialize system components
    vector<Airline> airlines;
    initializeAirlines(airlines);

    vector<Runway> runways;
    initializeRunways(runways);

    vector<Aircraft> activeAircrafts;

    cout << "AirControlX - Automated Air Traffic Control System" << endl;
    cout << "Simulation started for " << SIMULATION_DURATION << " seconds (" 
         << SIMULATION_DURATION/60 << " minutes)" << endl;

    // Main simulation loop
    auto startTime = chrono::steady_clock::now();
    int elapsedTime = 0;
    int aircraftSequence = 0;

    while (elapsedTime < SIMULATION_DURATION) {

        bool aircraftCreated = false;
        // Generate new flights based on time intervals
        if (elapsedTime % 180 == 0) { // Every 3 minutes (North - International Arrivals)
            Aircraft newAircraft = createAircraft(airlines, NORTH, ++aircraftSequence);
            activeAircrafts.push_back(newAircraft);
            aircraftCreated = true;
        }
        
        if (elapsedTime % 150 == 0) { // Every 2.5 minutes (East - International Departures)
            Aircraft newAircraft = createAircraft(airlines, EAST, ++aircraftSequence);
            activeAircrafts.push_back(newAircraft);
            aircraftCreated = true;
        }
        if (elapsedTime % 120 == 0 && elapsedTime != 0) { // Every 2 minutes (South - Domestic Arrivals)
            Aircraft newAircraft = createAircraft(airlines, SOUTH, ++aircraftSequence);
            activeAircrafts.push_back(newAircraft);
            aircraftCreated = true;
        }
        if (elapsedTime % 240 == 0 && elapsedTime != 0) { // Every 4 minutes (West - Domestic Departures)
            Aircraft newAircraft = createAircraft(airlines, WEST, ++aircraftSequence);
            activeAircrafts.push_back(newAircraft);
            aircraftCreated = true;
        }

        if (aircraftCreated)
        {
            displayStatus(activeAircrafts, runways, elapsedTime);
        }

        // Process each aircraft
        for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) {
            Aircraft& aircraft = *it;

            // Update aircraft speed and check for violations
            updateAircraftSpeed(aircraft);

            // Simulate based on direction (arrival or departure)
            if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
                simulateArrival(aircraft, runways);
            } else {
                simulateDeparture(aircraft, runways);
            }

          
            if (aircraft.isFaulty) 
                it = activeAircrafts.erase(it);           
            else 
                ++it;            
        }

        // Handle random ground faults
        if (elapsedTime % 30 == 0) { // Check for faults every 30 seconds
            handleGroundFaults(activeAircrafts);
        }

        // Check for invalid aircrafts every 7 seconds:
        if (elapsedTime % 7 == 0 && elapsedTime != 0) 
        {
            for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) 
            {
                if (it->phase == INVALID)
                    it = activeAircrafts.erase(it);
                else
                    ++it;                
            }
        }

        // Display current status and check for violations:
        displayStatus(activeAircrafts, runways, elapsedTime);
        for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ++it) {
            Aircraft& aircraft = *it;
            checkForViolations(aircraft);
        }

        // Wait for the next time step
        this_thread::sleep_for(chrono::milliseconds(TIME_STEP * 1000));
        elapsedTime = chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - startTime).count();
    }

    cout << "\nSimulation completed. Final statistics:" << endl;
    for (const auto& airline : airlines) {
        cout << airline.name << " violations: " << airline.violations << endl;
    }

    return 0;
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
    runways = {
        {RWY_A, "RWY-A (North-South Arrivals)", false, nullptr},
        {RWY_B, "RWY-B (East-West Departures)", false, nullptr},
        {RWY_C, "RWY-C (Cargo/Emergency/Overflow)", false, nullptr}
    };
}

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

Aircraft createAircraft(const vector<Airline>& airlines, Direction direction, int seq) {
    // Determine if this is an emergency flight based on direction probabilities
    bool isEmergency = false;
    int emergencyProb = 0;
    
    switch (direction) {
        case NORTH: emergencyProb = 10; break; // 10%
        case SOUTH: emergencyProb = 5; break;  // 5%
        case EAST: emergencyProb = 15; break; // 15%
        case WEST: emergencyProb = 20; break; // 20%
    }
    
    isEmergency = (rand() % 100) < emergencyProb;
    
    // Select airline based on type needed
    vector<const Airline*> emergencyEligibleAirlines;
    for (const auto& airline : airlines) 
    {
        if (isEmergency && (airline.type == EMERGENCY)) 
            emergencyEligibleAirlines.push_back(&airline);
        else if(!isEmergency && (airline.type != EMERGENCY)) 
            emergencyEligibleAirlines.push_back(&airline);            
    }

    // Select airline based on type needed
    vector<const Airline*> eligibleAirlines; 
    if (!isEmergency)
    {
        for (const auto& airline : emergencyEligibleAirlines) 
        {
            if (cargoCreated && airline->type == CARGO)
                continue;
            else
                eligibleAirlines.push_back(airline);
        }
    }

    if (eligibleAirlines.empty()) {
        // Fallback to any airline if none match (shouldn't happen with our data)
        eligibleAirlines.push_back(&airlines[0]);
    }
    
    const Airline* selectedAirline = eligibleAirlines[rand() % eligibleAirlines.size()];
    if (selectedAirline->type == CARGO) 
        cargoCreated = true; // Set flag to prevent further cargo creation
   
    Aircraft aircraft;
    aircraft.flightNumber = generateFlightNumber(*selectedAirline, seq);
    aircraft.airline = const_cast<Airline*>(selectedAirline);
    aircraft.type = selectedAirline->type;
    aircraft.direction = direction;
    aircraft.isEmergency = isEmergency;
    aircraft.hasAVN = false;
    aircraft.isFaulty = false;
    
    // Set initial phase based on direction
    if (direction == NORTH || direction == SOUTH) {
        aircraft.phase = HOLDING;
        aircraft.speed = 400 + rand() % 201; // 400-600 km/h
    } else {
        aircraft.phase = AT_GATE;
        aircraft.speed = 0;
    }
    
    return aircraft;
}

void simulateArrival(Aircraft& aircraft, vector<Runway>& runways) {
    switch (aircraft.phase) {
        case HOLDING:
            if (rand() % 10 == 0)  // Random transition from holding to approach
                aircraft.phase = APPROACH;            
            break;
            
        case APPROACH: {
            Runway* assignedRunway = assignRunway(aircraft, runways);
            if (assignedRunway != nullptr) {
                aircraft.phase = LANDING;
                assignedRunway->isOccupied = true;
                assignedRunway->currentAircraft = &aircraft;
            }
            break;
        }
            
        case LANDING:
            if (aircraft.speed <= 30) { // Simulate landing
                aircraft.phase = TAXI;  
                
                // Free the runway
                for (auto& runway : runways) {
                    if (runway.currentAircraft == &aircraft) {
                        runway.isOccupied = false;
                        runway.currentAircraft = nullptr;
                        break;
                    }
                }
            }
            break;
            
        case TAXI:
            if (rand() % 5 == 0) // Random transition to gate
                aircraft.phase = AT_GATE;
            break;
            
        case AT_GATE:
            if (aircraft.speed == 0)
                aircraft.phase = INVALID; // Mark for removal (will be removed in main after 0-10 seconds)
            break;
            
        default:
            break;
    }
}

void simulateDeparture(Aircraft& aircraft, vector<Runway>& runways) {
    switch (aircraft.phase) {
        case AT_GATE:
            if (rand() % 5 == 0) { // Random transition from gate to taxi
                aircraft.phase = TAXI;
                //aircraft.speed = 15 + rand() % 16; // 15-30 km/h
            }
            break;
            
        case TAXI: {
            Runway* assignedRunway = assignRunway(aircraft, runways);
            if (assignedRunway != nullptr) {
                aircraft.phase = TAKEOFF_ROLL;
               // aircraft.speed = 0; // Start of takeoff roll
                assignedRunway->isOccupied = true;
                assignedRunway->currentAircraft = &aircraft;
            }
            break;
        }
            
        case TAKEOFF_ROLL:
            // Simulate acceleration
            if (aircraft.speed < 290) {
                //aircraft.speed += 20; // Simple acceleration model
            } else {
                aircraft.phase = CLIMB;
                //aircraft.speed = 250 + rand() % 214; // 250-463 km/h
                
                // Free the runway
                for (auto& runway : runways) {
                    if (runway.currentAircraft == &aircraft) {
                        runway.isOccupied = false;
                        runway.currentAircraft = nullptr;
                        break;
                    }
                }
            }
            break;
            
        case CLIMB:
            if (rand() % 5 == 0) { // Random transition to cruise
                aircraft.phase = DEPARTURE_CRUISE;
                //aircraft.speed = 800 + rand() % 101; // 800-900 km/h
            }
            break;
            
        case DEPARTURE_CRUISE:
            // Aircraft remains here until removed from simulation
            break;
            
        default:
            break;
    }
}

Runway* assignRunway(Aircraft& aircraft, vector<Runway>& runways) {
    // Emergency flights get priority
    if (aircraft.isEmergency || aircraft.type == EMERGENCY || aircraft.type == CARGO) {
        if (!runways[RWY_C].isOccupied)
            return &runways[RWY_C];    
        return nullptr;
    }
    
    // Normal runway assignment based on direction
    if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
        if (!runways[RWY_A].isOccupied)
            return &runways[RWY_A];
        
    } else { // EAST or WEST
        if (!runways[RWY_B].isOccupied) 
            return &runways[RWY_B];
        
    }
    
    // If preferred runway is occupied, try overflow runway
    if (!runways[RWY_C].isOccupied) 
        return &runways[RWY_C];
    
    
    return nullptr; // No available runway
}

// Calculating speed given aircraft phase
int calculateSpeedChange(const Aircraft& aircraft)
{
    // Speed Change based on Phase:
    int speedChange;

    if(aircraft.phase == HOLDING || aircraft.phase == LANDING || aircraft.phase == TAKEOFF_ROLL || aircraft.phase == CLIMB)
        speedChange = rand() % 51;

    else if(aircraft.phase == TAXI)
        speedChange = rand() % 9;

    else if(aircraft.phase == AT_GATE)
        speedChange = rand() % 4;

    else if(aircraft.phase == APPROACH)
        speedChange = rand() % 16;

    else if(aircraft.phase == DEPARTURE_CRUISE)
        speedChange = rand() % 20;

    else
        speedChange = 0; 

    return speedChange;
}

void updateAircraftSpeed(Aircraft& aircraft) 
{    
    int speedChange = calculateSpeedChange(aircraft);
    if (aircraft.direction == NORTH || aircraft.direction == SOUTH)
        aircraft.speed -= speedChange;
    else
        aircraft.speed += speedChange;

    /* Avoiding negative aircraft speeds and ensuring somewhat speed in TAXI Phase 
    given that only landing phase can drop the speed so low to be negative) */
    if (aircraft.speed < 0) 
    {
        if (aircraft.phase != AT_GATE)
            aircraft.speed = (rand() % 10) + 10;
        else
            aircraft.speed = 0;
    }
    // if (aircraft.speed > 1000) aircraft.speed -= 50; // Max speed limit    
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

void handleGroundFaults(vector<Aircraft>& aircrafts) {
    for (auto& aircraft : aircrafts) {
        // Only check aircraft on ground (taxi or at gate)
        if ((aircraft.phase == TAXI || aircraft.phase == AT_GATE) && !aircraft.isFaulty) {
            // 5% chance of fault
            if (rand() % 100 < 5) {
                aircraft.isFaulty = true;
                
                lock_guard<mutex> guard(coutMutex);
                cout << "FAULT DETECTED: " << aircraft.flightNumber << " (" 
                     << aircraft.airline->name << ") has a ground fault and is being towed." << endl;
            }
        }
    }
}

void displayStatus(const vector<Aircraft>& aircrafts, const vector<Runway>& runways, int elapsedTime) {
    lock_guard<mutex> guard(coutMutex);
    
    // Convert elapsed time to minutes:seconds
    int minutes = elapsedTime / 60;
    int seconds = elapsedTime % 60;
    
    cout << "\n\n=== AirControlX Status at " << setw(2) << setfill('0') << minutes 
         << ":" << setw(2) << setfill('0') << seconds << " ===" << endl;
    
    // Display runway status
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
    
    // Display aircraft status
    cout << "\nActive Aircraft (" << aircrafts.size() << "):" << endl;
    for (const auto& aircraft : aircrafts) {
        if (aircraft.isFaulty) continue; // Skip faulty aircraft
        
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
        case INVALID: return "At Gate (being removed soon)";
        default: return "Unknown";
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
