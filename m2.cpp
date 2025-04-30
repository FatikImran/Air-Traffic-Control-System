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
#include <queue>
#include <algorithm>

using namespace std;

// Constants
const int SIMULATION_DURATION = 300; 		// 5 minutes in seconds
const int TIME_STEP = 1; 			// 1 second per simulation step

// Enums
enum AircraftType { COMMERCIAL, CARGO, EMERGENCY };
enum FlightPhase {
    HOLDING, APPROACH, LANDING, TAXI, AT_GATE, 	// Arrival phases
    TAKEOFF_ROLL, CLIMB, DEPARTURE_CRUISE,      // Departure phases
    AT_GATE_BRS, DEPARTURE_CRUISE_BRS,
    ACCELERATING_TO_CRUISE,
};
enum Direction { NORTH, SOUTH, EAST, WEST };
enum RunwayID { RWY_A, RWY_B, RWY_C };
enum Priority { EMERGENCY_PRIORITY, HIGH_PRIORITY, NORMAL_PRIORITY };

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
    int timeInFlight = 0; // seconds
    int fuelStatus; // percentage
    Priority priority;
    time_t scheduledTime;
    int estimatedWaitTime = 0;
    string status = "Waiting";
};

// Runway structure
struct Runway {
    RunwayID id;
    string name;
    bool isOccupied;
    Aircraft* currentAircraft;
    queue<Aircraft*> waitingQueue;
};

// Comparison function for priority queue
struct ComparePriority {
    bool operator()(Aircraft* a, Aircraft* b) {
        if (a->priority == b->priority) {
            return a->scheduledTime > b->scheduledTime; // FCFS within same priority
        }
        return a->priority > b->priority; // Lower value means higher priority
    }
};

// Global variables
mutex coutMutex;
bool cargoCreated = false;
priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> arrivalQueue;
priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> departureQueue;
map<string, Aircraft*> aircraftStatusMap;

// Function prototypes
void initializeAirlines(vector<Airline>& airlines);
void initializeRunways(vector<Runway>& runways);
string generateFlightNumber(const Airline& airline, int sequence);
Aircraft* createAircraft(const vector<Airline>& airlines, Direction direction, int seq);
void simulateArrival(Aircraft& aircraft, vector<Runway>& runways);
void simulateDeparture(Aircraft& aircraft, vector<Runway>& runways);
void updateAircraftSpeed(Aircraft& aircraft);
void checkForViolations(Aircraft& aircraft);
void displayStatus(const vector<Aircraft*>& aircrafts, const vector<Runway>& runways, int elapsedTime);
void handleGroundFaults(vector<Aircraft*>& aircrafts);
Runway* assignRunway(Aircraft& aircraft, vector<Runway>& runways);
string getPhaseName(FlightPhase phase);
string getDirectionName(Direction dir);
string getAircraftTypeName(AircraftType type);
// void manageRunwayQueues(vector<Runway>& runways);
void updateWaitTimes();
bool processFlightEntry(Direction direction, vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence, int elapsedTime);
void updateAircraftStatus(Aircraft& aircraft);

int main() {
    srand(time(0));

    // Initialize system components
    vector<Airline> airlines;
    initializeAirlines(airlines);

    vector<Runway> runways;
    initializeRunways(runways);

    vector<Aircraft*> activeAircrafts;

    cout << "AirControlX - Automated Air Traffic Control System" << endl;
    cout << "Simulation started for " << SIMULATION_DURATION << " seconds ("
        << SIMULATION_DURATION / 60 << " minutes)" << endl;

    // Main simulation loop
    auto startTime = chrono::steady_clock::now();
    int elapsedTime = 0;
    int aircraftSequence = 0;

    while (elapsedTime < SIMULATION_DURATION) {

        bool northCreated = false, southCreated = false, eastCreated = false, westCreated = false;
        // Generate new flights based on time intervals
        northCreated = processFlightEntry(NORTH, airlines, activeAircrafts, aircraftSequence, elapsedTime);
        southCreated = processFlightEntry(SOUTH, airlines, activeAircrafts, aircraftSequence, elapsedTime);
        eastCreated = processFlightEntry(EAST, airlines, activeAircrafts, aircraftSequence, elapsedTime);
        westCreated = processFlightEntry(WEST, airlines, activeAircrafts, aircraftSequence, elapsedTime);

        // Check if any aircraft was created and print its status at creation
        if (northCreated || southCreated || eastCreated || westCreated)
            displayStatus(activeAircrafts, runways, elapsedTime);

        updateWaitTimes();

        // Process each aircraft
        for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) {
            Aircraft* aircraft = *it;

            // Update aircraft status
            updateAircraftStatus(*aircraft);

            // Update aircraft speed and check for violations
            updateAircraftSpeed(*aircraft);

            // Simulate based on direction (arrival or departure)
            if (aircraft->direction == NORTH || aircraft->direction == SOUTH) {
                simulateArrival(*aircraft, runways);
            }
            else {
                simulateDeparture(*aircraft, runways);
            }

            if (aircraft->isFaulty) {
                // Remove from any runway queues
                for (auto& runway : runways) {
                    queue<Aircraft*> tempQueue;
                    while (!runway.waitingQueue.empty()) {
                        Aircraft* queuedAircraft = runway.waitingQueue.front();
                        runway.waitingQueue.pop();
                        if (queuedAircraft != aircraft) {
                            tempQueue.push(queuedAircraft);
                        }
                    }
                    runway.waitingQueue = tempQueue;
                }
                it = activeAircrafts.erase(it);
                delete aircraft;
            }
            else {
                ++it;
            }
        }

        // Handle random ground faults
        if (elapsedTime % 30 == 0) { // Check for faults every 30 seconds
            handleGroundFaults(activeAircrafts);
        }

        // Check for aircrafts to be removed
        if (elapsedTime % 7 == 0 && elapsedTime != 0) {
            for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) {
                if ((*it)->phase == AT_GATE_BRS || (*it)->phase == DEPARTURE_CRUISE_BRS) {
                    it = activeAircrafts.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        // Display current status and check for violations:
        displayStatus(activeAircrafts, runways, elapsedTime);
        for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ++it) {
            Aircraft* aircraft = *it;
            checkForViolations(*aircraft);
        }

        // Wait for the next time step
        this_thread::sleep_for(chrono::milliseconds(TIME_STEP * 1000));
        elapsedTime = chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - startTime).count();

        // Updating time in flight for each aircraft
        for (auto& aircraft : activeAircrafts) {
            aircraft->timeInFlight++;
        }
    }

    cout << "\nSimulation completed. Final statistics:" << endl;
    for (const auto& airline : airlines) {
        cout << airline.name << " violations: " << airline.violations << endl;
    }

    // Clean up remaining aircraft objects
    for (auto aircraft : activeAircrafts) {
        delete aircraft;
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

Aircraft* createAircraft(const vector<Airline>& airlines, Direction direction, int seq)
{
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
        else if (!isEmergency && (airline.type != EMERGENCY))
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

    Aircraft* aircraft = new Aircraft();
    aircraft->flightNumber = generateFlightNumber(*selectedAirline, seq);
    aircraft->airline = const_cast<Airline*>(selectedAirline);
    aircraft->type = selectedAirline->type;
    aircraft->direction = direction;
    aircraft->isEmergency = isEmergency;
    aircraft->hasAVN = false;
    aircraft->isFaulty = false;
    aircraft->fuelStatus = 20 + rand() % 60; // Random fuel status between 20-80%
    aircraft->scheduledTime = time(nullptr);

    // Set priority
    if (aircraft->isEmergency || aircraft->type == EMERGENCY) {
        aircraft->priority = EMERGENCY_PRIORITY;
    }
    else if (aircraft->type == CARGO || aircraft->fuelStatus < 30) {
        aircraft->priority = HIGH_PRIORITY;
    }
    else {
        aircraft->priority = NORMAL_PRIORITY;
    }

    // Set initial phase based on direction
    if (direction == NORTH || direction == SOUTH) {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        arrivalQueue.push(aircraft);
    }
    else {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        departureQueue.push(aircraft);
    }

    // Add to status map
    aircraftStatusMap[aircraft->flightNumber] = aircraft;

    return aircraft;
}

bool processFlightEntry(Direction direction, vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence, int elapsedTime)
{
    bool shouldCreate = false;

    switch (direction) {
    case NORTH: shouldCreate = (elapsedTime % 180 == 0); break; // Every 3 minutes
    case SOUTH: shouldCreate = (elapsedTime % 120 == 0 && elapsedTime != 0); break; // Every 2 minutes
    case EAST: shouldCreate = (elapsedTime % 150 == 0); break; // Every 2.5 minutes
    case WEST: shouldCreate = (elapsedTime % 240 == 0 && elapsedTime != 0); break; // Every 4 minutes
    }

    if (shouldCreate) {
        Aircraft* newAircraft = createAircraft(airlines, direction, ++aircraftSequence);
        activeAircrafts.push_back(newAircraft);

        lock_guard<mutex> guard(coutMutex);
        cout << "New flight " << newAircraft->flightNumber << " (" << newAircraft->airline->name
            << ") scheduled from " << getDirectionName(direction) << " direction." << endl;
    }
    return shouldCreate;
}

//void manageRunwayQueues(vector<Runway>& runways)
//{
//    // Process arrival queue
//    while (!arrivalQueue.empty())
//    {
//        Aircraft* aircraft = arrivalQueue.top();
//        Runway* assignedRunway = assignRunway(*aircraft, runways);
//
//        if (assignedRunway)
//        {
//            if (!assignedRunway->isOccupied)
//            {
//                assignedRunway->isOccupied = true;
//                assignedRunway->currentAircraft = aircraft;
//                aircraft->status = "Approaching Runway";
//            }
//            else
//            {
//                assignedRunway->waitingQueue.push(aircraft);
//                aircraft->status = "Waiting for Runway";
//            }
//            arrivalQueue.pop();
//        }
//        else
//            break; // No available runway
//
//    }
//
//    // Process departure queue
//    while (!departureQueue.empty()) {
//        Aircraft* aircraft = departureQueue.top();
//        Runway* assignedRunway = assignRunway(*aircraft, runways);
//
//        if (assignedRunway) {
//            if (!assignedRunway->isOccupied) {
//                assignedRunway->isOccupied = true;
//                assignedRunway->currentAircraft = aircraft;
//                aircraft->status = "Taking Off";
//            }
//            else {
//                assignedRunway->waitingQueue.push(aircraft);
//                aircraft->status = "Waiting for Runway";
//            }
//            departureQueue.pop();
//        }
//        else {
//            break; // No available runway
//        }
//    }
//}
void simulateArrival(Aircraft& aircraft, vector<Runway>& runways) {
    switch (aircraft.phase) {
    case HOLDING:
        if (aircraft.timeInFlight >= 5) {
            aircraft.phase = APPROACH;
            aircraft.status = "Approaching";
        }
        break;

    case APPROACH: {
        Runway* assignedRunway = assignRunway(aircraft, runways);
        if (assignedRunway && aircraft.timeInFlight >= 9) {
            aircraft.phase = LANDING;
            aircraft.status = "Landing";           
        }
        break;
    }

    case LANDING:
        if (aircraft.timeInFlight >= 14) {
            aircraft.phase = TAXI;
            aircraft.status = "Taxiing";

            // Free the runway and assign next aircraft
            for (auto& runway : runways) {
                if (runway.currentAircraft == &aircraft) {
                    runway.isOccupied = false;
                    runway.currentAircraft = nullptr;

                    // Assign next aircraft from queue if available
                    if (!runway.waitingQueue.empty()) {
                        Aircraft* nextAircraft = runway.waitingQueue.front();
                        runway.waitingQueue.pop();
                        runway.currentAircraft = nextAircraft;
                        runway.isOccupied = true;
                        nextAircraft->status = "Approaching Runway";
                    }
                    break;
                }
            }
        }
        break;

    case TAXI:
        if (aircraft.timeInFlight >= 17) {
            aircraft.phase = AT_GATE;
            aircraft.status = "At Gate";
        }
        break;

    case AT_GATE:
        if (aircraft.timeInFlight >= 20 && aircraft.speed <= 0) {
            aircraft.phase = AT_GATE_BRS;
            aircraft.status = "Completed";
        }
        break;

    default:
        break;
    }
}

void simulateDeparture(Aircraft& aircraft, vector<Runway>& runways) {
    switch (aircraft.phase) {
    case AT_GATE:
        if (aircraft.timeInFlight >= 3) {
            aircraft.phase = TAXI;
            aircraft.status = "Taxiing";
        }
        break;

    case TAXI: {
        Runway* assignedRunway = assignRunway(aircraft, runways);
        if (assignedRunway && aircraft.timeInFlight >= 6) {
            aircraft.phase = TAKEOFF_ROLL;
            aircraft.status = "Taking Off";
            assignedRunway->isOccupied = true;
            assignedRunway->currentAircraft = &aircraft;
        }
        break;
    }

    case TAKEOFF_ROLL:
        if (aircraft.timeInFlight >= 12)
        {
            // Changing Phase & Freeing the runway
            aircraft.phase = CLIMB;
            aircraft.status = "Climbing";
            for (auto& runway : runways)
            {
                if (runway.currentAircraft == &aircraft)
                {
                    runway.isOccupied = false;
                    runway.currentAircraft = nullptr;

                    // Assign next aircraft from queue if available
                    if (!runway.waitingQueue.empty()) {
                        Aircraft* nextAircraft = runway.waitingQueue.front();
                        runway.waitingQueue.pop();
                        runway.currentAircraft = nextAircraft;
                        runway.isOccupied = true;
                        nextAircraft->status = "Taking Off";
                    }
                    break;
                }
            }
        }
        break;

    case CLIMB:
        if (aircraft.timeInFlight >= 16) {
            aircraft.phase = ACCELERATING_TO_CRUISE;
            aircraft.status = "Accelerating";
        }
        break;

    case ACCELERATING_TO_CRUISE:
        if (aircraft.timeInFlight >= 23) {
            aircraft.phase = DEPARTURE_CRUISE;
            aircraft.status = "Cruising";
        }
        break;

    case DEPARTURE_CRUISE:
        if (aircraft.timeInFlight >= 25) {
            aircraft.phase = DEPARTURE_CRUISE_BRS;
            aircraft.status = "Completed";
        }
        break;

    default:
        break;
    }
}

Runway* assignRunway(Aircraft& aircraft, vector<Runway>& runways) {
	// Check if the aircraft is already assigned to a runway
	for (auto& runway : runways) {
		if (runway.currentAircraft == &aircraft) {
			return &runway;
		}
	}
	// Assign runway based on aircraft type and direction
	Runway* assignedRunway = nullptr;
    // Emergency flights get priority on any runway
    if (aircraft.priority == EMERGENCY_PRIORITY) 
    {
        // Try RWY_C first
		if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
		else if (!runways[RWY_A].isOccupied) assignedRunway = &runways[RWY_A];
		else if (!runways[RWY_B].isOccupied) assignedRunway = &runways[RWY_B];
        else
        {
			// If all runways are occupied, assign to the one with the least waiting time
			assignedRunway = &runways[RWY_C];
			for (auto& runway : runways) 
            {
				if (runway.waitingQueue.size() < assignedRunway->waitingQueue.size())
					assignedRunway = &runway;
			}
			// Add the aircraft to the waiting queue
			assignedRunway->waitingQueue.push(&aircraft);
			aircraft.status = "Waiting for Runway";
            return nullptr;
        }
		assignedRunway->isOccupied = true;
		assignedRunway->currentAircraft = &aircraft;
        return assignedRunway;
    }

    // Cargo flights must use RWY_C
    if (aircraft.type == CARGO) {
		if (!runways[RWY_C].isOccupied)
        {
			assignedRunway = &runways[RWY_C];
			assignedRunway->isOccupied = true;
			assignedRunway->currentAircraft = &aircraft;
			return assignedRunway;
		}
		else 
        {
			// Add to waiting queue
			runways[RWY_C].waitingQueue.push(&aircraft);
			aircraft.status = "Waiting for Runway";
			return nullptr;
		}
    }

    // Arrival runway assignment (NORTH/SOUTH on RWY-A or RWY-C)
    if (aircraft.direction == NORTH || aircraft.direction == SOUTH) 
    {
		Runway* assignedRunway = nullptr;
		if (!runways[RWY_A].isOccupied) assignedRunway = &runways[RWY_A];
		else if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
        else
        {
			// If all runways are occupied, assign to the one with the least waiting time
			assignedRunway = &runways[RWY_A];
			if (runways[RWY_C].waitingQueue.size() < assignedRunway->waitingQueue.size())
				assignedRunway = &runways[RWY_C];
			assignedRunway->waitingQueue.push(&aircraft);
			aircraft.status = "Waiting for Runway";
			return nullptr;
		}
		assignedRunway->isOccupied = true;
		assignedRunway->currentAircraft = &aircraft;
		return assignedRunway;
    }
    // Departure runway assignment (EAST/WEST on RWY-B or RWY-C)
    else 
    {
        Runway* assignedRunway = nullptr;
        if (!runways[RWY_B].isOccupied) assignedRunway = &runways[RWY_B];
        else if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
        else
        {
            // If all runways are occupied, assign to the one with the least waiting time
            assignedRunway = &runways[RWY_B];
            if (runways[RWY_C].waitingQueue.size() < assignedRunway->waitingQueue.size())
                assignedRunway = &runways[RWY_C];
            assignedRunway->waitingQueue.push(&aircraft);
            aircraft.status = "Waiting for Runway";
            return nullptr;
        }
        assignedRunway->isOccupied = true;
        assignedRunway->currentAircraft = &aircraft;
        return assignedRunway;
    }    
}

void updateWaitTimes() {
    // Update wait times for all aircraft in queues
    for (auto& pair : aircraftStatusMap) {
        Aircraft* aircraft = pair.second;
        if (aircraft->status == "Waiting for Runway") {
            aircraft->estimatedWaitTime++;
        }
    }
}

void updateAircraftStatus(Aircraft& aircraft) {
    switch (aircraft.phase) {
    case HOLDING: aircraft.status = "Holding Pattern"; break;
    case APPROACH: aircraft.status = "Approaching"; break;
    case LANDING: aircraft.status = "Landing"; break;
    case TAXI: aircraft.status = "Taxiing"; break;
    case AT_GATE: aircraft.status = "At Gate"; break;
    case TAKEOFF_ROLL: aircraft.status = "Taking Off"; break;
    case CLIMB: aircraft.status = "Climbing"; break;
    case DEPARTURE_CRUISE: aircraft.status = "Cruising"; break;
    case ACCELERATING_TO_CRUISE: aircraft.status = "Accelerating"; break;
    default: break;
    }
}
// Calculating speed given aircraft phase
int calculateSpeedChange(const Aircraft& aircraft)
{
    // Speed Change based on Phase:
    int speedChange;

    if (aircraft.phase == TAKEOFF_ROLL || aircraft.phase == CLIMB)
        speedChange = rand() % 18 + 30;

    else if (aircraft.phase == LANDING)
        speedChange = 38 + rand() % 10;

    else if (aircraft.phase == HOLDING)
        speedChange = rand() % 10 + 42;

    else if (aircraft.phase == TAXI)
        speedChange = rand() % 5 + 4;

    else if (aircraft.phase == AT_GATE)
        speedChange = rand() % 3 + 2;

    else if (aircraft.phase == APPROACH)
        speedChange = rand() % 5 + 11;

    else if (aircraft.phase == ACCELERATING_TO_CRUISE)
        speedChange = rand() % 26 + 50;

    else if (aircraft.phase == DEPARTURE_CRUISE)
        speedChange = rand() % 8 + 18;

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
    case ACCELERATING_TO_CRUISE:
        violation = false;
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

void handleGroundFaults(vector<Aircraft*>& aircrafts) {
    for (auto& aircraft : aircrafts) {
        // Only check aircraft on ground (taxi or at gate)
        if ((aircraft->phase == TAXI || aircraft->phase == AT_GATE) && !aircraft->isFaulty) {
            // 5% chance of fault
            if (rand() % 100 < 5) {
                aircraft->isFaulty = true;

                lock_guard<mutex> guard(coutMutex);
                cout << "FAULT DETECTED: " << aircraft->flightNumber << " ("
                    << aircraft->airline->name << ") has a ground fault and is being towed." << endl;
            }
        }
    }
}

void displayStatus(const vector<Aircraft*>& aircrafts, const vector<Runway>& runways, int elapsedTime) {
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
        }
        else {
            cout << "Available";
        }

        // Display waiting queue
        if (!runway.waitingQueue.empty()) {
            cout << " | Waiting: " << runway.waitingQueue.size() << " aircraft";
        }
        cout << endl;
    }

    // Display arrival and departure queues
    cout << "\nArrival Queue: " << arrivalQueue.size() << " aircraft" << endl;
    cout << "Departure Queue: " << departureQueue.size() << " aircraft" << endl;

    // Display aircraft status
    cout << "\nActive Aircraft (" << aircrafts.size() << "):" << endl;
    for (const auto& aircraft : aircrafts) {
        if (aircraft->isFaulty) continue; // Skip faulty aircraft

        cout << aircraft->flightNumber << " (" << aircraft->airline->name << "): "
            << getDirectionName(aircraft->direction) << " "
            << (aircraft->direction == NORTH || aircraft->direction == SOUTH ? "Arrival" : "Departure")
            << ", " << getAircraftTypeName(aircraft->type)
            << (aircraft->isEmergency ? " [EMERGENCY]" : "")
            << ", Status: " << aircraft->status
            << ", Phase: " << getPhaseName(aircraft->phase)
            << ", Speed: " << aircraft->speed << " km/h"
            << ", Fuel: " << aircraft->fuelStatus << "%"
            << ", Priority: ";

        switch (aircraft->priority) {
        case EMERGENCY_PRIORITY: cout << "Emergency"; break;
        case HIGH_PRIORITY: cout << "High"; break;
        case NORMAL_PRIORITY: cout << "Normal"; break;
        }

        if (aircraft->status == "Waiting for Runway") {
            cout << ", Wait Time: " << aircraft->estimatedWaitTime << "s";
        }

        cout << (aircraft->hasAVN ? " [AVN ISSUED]" : "") << endl;
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
    case AT_GATE_BRS: return "At Gate (being removed soon)";
    case DEPARTURE_CRUISE_BRS: return "Departure Cruise (being removed soon)";
    case ACCELERATING_TO_CRUISE: return "Accelerating to Cruise";
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