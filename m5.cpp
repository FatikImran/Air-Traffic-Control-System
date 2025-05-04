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
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

// Constants
const int SIMULATION_DURATION = 300;        // 5 minutes in seconds
const int TIME_STEP = 1;                   // 1 second per simulation step
const string ADMIN_USERNAME = "admin";
const string ADMIN_PASSWORD = "atcs2025";

// Enums
enum AircraftType { COMMERCIAL, CARGO, EMERGENCY };
enum FlightPhase {
    HOLDING, APPROACH, LANDING, TAXI, AT_GATE,     // Arrival phases
    TAKEOFF_ROLL, CLIMB, DEPARTURE_CRUISE,         // Departure phases
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
mutex simulationMutex;
bool cargoCreated = false;
priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> arrivalQueue;
priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> departureQueue;
map<string, Aircraft*> aircraftStatusMap;
bool simulationRunning = false;
bool simulationPaused = true; // Start paused until shown
thread simulationThread;
int globalElapsedTime = 0; // Track simulation time globally

// Function prototypes
void displayWelcomeScreen();
bool login();
void displayMenu();
void spawnCustomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void spawnRandomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void startSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void runSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void showSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void removeAircraftFromEverywhere(Aircraft* aircraft, vector<Aircraft*>& aircrafts, vector<Runway>& runways);
void initializeAirlines(vector<Airline>& airlines);
void initializeRunways(vector<Runway>& runways);
string generateFlightNumber(const Airline& airline, int sequence);
Aircraft* createAircraftForManualEntry(vector<Airline>& airlines, Direction direction, int seq, string flightNum, AircraftType type, Priority priority, time_t schedTime);
Aircraft* createAircraftForAutoEntry(const vector<Airline>& airlines, Direction direction, int seq);
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
void updateWaitTimes();
void updateAircraftStatus(Aircraft& aircraft);
bool kbhit();
void updateSimulationStep(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts);

// Set terminal to non-blocking input mode
void setNonBlockingInput(bool enable) {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    if (enable) {
        ttystate.c_lflag &= ~(ICANON | ECHO);
        ttystate.c_cc[VMIN] = 0;
        ttystate.c_cc[VTIME] = 0;
    } else {
        ttystate.c_lflag |= (ICANON | ECHO);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (enable) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }
}

int main() {
    srand(time(0));

    // Initialize system components
    vector<Airline> airlines;
    vector<Runway> runways;
    vector<Aircraft*> activeAircrafts;
    int aircraftSequence = 0;

    // Display welcome screen
    displayWelcomeScreen();

    // Login
    if (!login()) {
        cout << "Too many failed attempts. Exiting..." << endl;
        return 1;
    }

    // Main menu loop
    while (true) {
        setNonBlockingInput(false);
        displayMenu();
        int choice;
        cout << "Enter your choice: ";
        cin >> choice;
        cin.ignore(); // Clear newline

        switch (choice) {
            case 1: // Start Simulation
                if (!simulationRunning) {
                    initializeAirlines(airlines);
                    initializeRunways(runways);
                    simulationRunning = true;
                    simulationPaused = true; // Start paused
                    simulationThread = thread(runSimulation, ref(airlines), ref(runways), ref(activeAircrafts), ref(aircraftSequence));
                    simulationThread.detach();
                    cout << "Simulation started (paused until shown).\n";
                } else {
                    cout << "Simulation is already running!\n";
                }
                break;
            case 2: // End Simulation
                if (simulationRunning) {
                    lock_guard<mutex> guard(simulationMutex);
                    simulationRunning = false;
                    simulationPaused = true;
                    globalElapsedTime = 0;
                    cout << "Simulation terminated.\n";
                    // Clean up
                    for (auto aircraft : activeAircrafts)
                        removeAircraftFromEverywhere(aircraft, activeAircrafts, runways);
                    activeAircrafts.clear();
                    airlines.clear();
                    runways.clear();
                    aircraftStatusMap.clear();
                } else {
                    cout << "No simulation is running!\n";
                }
                break;
            case 3: // Spawn Custom Aircraft
                if (simulationRunning) {
                    lock_guard<mutex> guard(simulationMutex);
                    simulationPaused = true;
                    spawnCustomAircraft(airlines, activeAircrafts, aircraftSequence);
                } else {
                    cout << "Please start the simulation first!\n";
                }
                break;
            case 4: // Spawn Random Aircraft
                if (simulationRunning) {
                    lock_guard<mutex> guard(simulationMutex);
                    simulationPaused = true;
                    spawnRandomAircraft(airlines, activeAircrafts, aircraftSequence);
                } else {
                    cout << "Please start the simulation first!\n";
                }
                break;
            case 5: // Show Simulation
                if (simulationRunning) {
                    lock_guard<mutex> guard(simulationMutex);
                    simulationPaused = false;
                    showSimulation(airlines, runways, activeAircrafts, aircraftSequence);
                    simulationPaused = true;
                } else {
                    cout << "No simulation is running!\n";
                }
                break;
            case 6: // Exit
                if (simulationRunning) {
                    lock_guard<mutex> guard(simulationMutex);
                    simulationRunning = false;
                    simulationPaused = true;
                    for (auto aircraft : activeAircrafts)
                        removeAircraftFromEverywhere(aircraft, activeAircrafts, runways);
                }
                cout << "Exiting AirControlX. Goodbye!\n";
                return 0;
            default:
                cout << "Invalid choice. Please try again.\n";
        }
    }

    return 0;
}

void displayWelcomeScreen() {
    cout << "\033[1;36m"; // Cyan color
    cout << R"(
    ╔══════════════════════════════════════════════════════╗
    ║                                                      ║
    ║        █████╗ ██╗  ██╗    ███████╗ █████╗ ████████╗ ██████╗ ███████╗        ║
    ║       ██╔══██╗██║ ██╔╝    ██╔════╝██╔══██╗╚══██╔══╝██╔════╝ ██╔════╝        ║
    ║      ███████║█████╔╝     █████╗  ███████║   ██║   ██║  ███╗███████╗         ║
    ║      ██╔══██║██╔═██╗     ██╔══╝  ██╔══██║   ██║   ██║   ██║╚════██║         ║
    ║      ██║  ██║██║  ██╗    ██║     ██║  ██║   ██║   ╚██████╔╝███████║         ║
    ║      ╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝     ╚═╝  ╚═╝   ╚═╝    ╚═════╝ ╚══════╝         ║
    ║                                                      ║
    ║           Automated Air Traffic Control System        ║
    ║           Developed by: Muhammad Fatik & Kaleem      ║
    ║                                                      ║
    ╚══════════════════════════════════════════════════════╝
    )" << endl;
    cout << "\033[0m"; // Reset color
    cout << "Press Enter to continue...";
    cin.get();
}

bool login() {
    int attempts = 3;
    string username, password;

    while (attempts > 0) {
        cout << "\n=== AirControlX Login ===\n";
        cout << "Username: ";
        getline(cin, username);
        cout << "Password: ";
        getline(cin, password);

        if (username == ADMIN_USERNAME && password == ADMIN_PASSWORD) {
            cout << "Login successful!\n" << endl;
            return true;
        } else {
            attempts--;
            cout << "Invalid credentials. " << attempts << " attempts remaining.\n" << endl;
        }
    }
    return false;
}

void displayMenu() {
    cout << "\n=== AirControlX Main Menu ===\n";
    cout << "1. Start Simulation\n";
    cout << "2. End Simulation\n";
    cout << "3. Spawn Custom Aircraft\n";
    cout << "4. Spawn Random Aircraft\n";
    cout << "5. Show Simulation\n";
    cout << "6. Exit\n";
}

void spawnCustomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    cout << "\n=== Spawn Custom Aircraft ===\n";

    // Display available airlines
    cout << "Available Airlines:\n";
    for (size_t i = 0; i < airlines.size(); ++i) {
        cout << i + 1 << ". " << airlines[i].name << " (" << getAircraftTypeName(airlines[i].type) << ")\n";
    }

    // Get airline choice
    int airlineChoice;
    cout << "Select airline (1-" << airlines.size() << "): ";
    cin >> airlineChoice;
    cin.ignore();
    if (airlineChoice < 1 || airlineChoice > static_cast<int>(airlines.size())) {
        cout << "Invalid airline selection.\n";
        return;
    }
    Airline* selectedAirline = &airlines[airlineChoice - 1];

    // Get flight number
    string flightNumber;
    cout << "Enter flight number (e.g., PK101): ";
    getline(cin, flightNumber);

    // Get direction
    cout << "Select direction (1. North, 2. South, 3. East, 4. West): ";
    int dirChoice;
    cin >> dirChoice;
    cin.ignore();
    Direction direction;
    switch (dirChoice) {
        case 1: direction = NORTH; break;
        case 2: direction = SOUTH; break;
        case 3: direction = EAST; break;
        case 4: direction = WEST; break;
        default:
            cout << "Invalid direction.\n";
            return;
    }

    // Get priority
    cout << "Select priority (1. Emergency, 2. High, 3. Normal): ";
    int priChoice;
    cin >> priChoice;
    cin.ignore();
    Priority priority;
    switch (priChoice) {
        case 1: priority = EMERGENCY_PRIORITY; break;
        case 2: priority = HIGH_PRIORITY; break;
        case 3: priority = NORMAL_PRIORITY; break;
        default:
            cout << "Invalid priority.\n";
            return;
    }

    // Get scheduled time
    string schedTimeStr;
    cout << "Enter scheduled time (YYYY-MM-DD HH:MM): ";
    getline(cin, schedTimeStr);
    struct tm tm = {};
    istringstream ss(schedTimeStr);
    ss >> get_time(&tm, "%Y-%m-%d %H:%M");
    if (ss.fail()) {
        cout << "Invalid time format.\n";
        return;
    }
    time_t scheduledTime = mktime(&tm);

    // Create aircraft
    Aircraft* newAircraft = createAircraftForManualEntry(airlines, direction, ++aircraftSequence, flightNumber, selectedAirline->type, priority, scheduledTime);
    activeAircrafts.push_back(newAircraft);

    cout << "Flight " << flightNumber << " spawned successfully.\n";
}

void spawnRandomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    cout << "\n=== Spawn Random Aircraft ===\n";

    // Randomly select direction
    Direction direction = static_cast<Direction>(rand() % 4); // NORTH, SOUTH, EAST, WEST

    // Create aircraft
    Aircraft* newAircraft = createAircraftForAutoEntry(airlines, direction, ++aircraftSequence);
    activeAircrafts.push_back(newAircraft);

    cout << "Random flight " << newAircraft->flightNumber << " (" << newAircraft->airline->name
         << ") spawned from " << getDirectionName(direction) << " direction.\n";
}

void startSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    lock_guard<mutex> guard(simulationMutex);
    if (!simulationRunning) {
        simulationRunning = true;
        simulationPaused = true; // Start paused
        simulationThread = thread(runSimulation, ref(airlines), ref(runways), ref(activeAircrafts), ref(aircraftSequence));
        simulationThread.detach();
    }
}

void runSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    while (simulationRunning && globalElapsedTime < SIMULATION_DURATION) {
        {
            lock_guard<mutex> guard(simulationMutex);
            if (simulationPaused) {
                this_thread::sleep_for(chrono::milliseconds(100));
                continue;
            }
        }
        // When not paused, simulation advances in showSimulation
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    lock_guard<mutex> guard(simulationMutex);
    if (simulationRunning) {
        simulationRunning = false;
        simulationPaused = true;
        cout << "\nSimulation completed. Final statistics:" << endl;
        for (const auto& airline : airlines)
            cout << airline.name << " violations: " << airline.violations << endl;
    }
}

void showSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    setNonBlockingInput(true);
    cout << "Showing simulation. Press 'q' to return to menu.\n";

    while (simulationRunning && globalElapsedTime < SIMULATION_DURATION) {
        {
            lock_guard<mutex> guard(simulationMutex);
            if (kbhit()) {
                cout << "Returning to menu.\n";
                break;
            }
            displayStatus(activeAircrafts, runways, globalElapsedTime);
            updateSimulationStep(airlines, runways, activeAircrafts);
            globalElapsedTime += TIME_STEP;
        }
        this_thread::sleep_for(chrono::milliseconds(TIME_STEP * 1000));
    }

    setNonBlockingInput(false);
}

void updateSimulationStep(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts) {
    updateWaitTimes();

    // Process each aircraft
    for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) {
        Aircraft* aircraft = *it;

        // Update aircraft status
        updateAircraftStatus(*aircraft);

        // Update aircraft speed and check for violations
        updateAircraftSpeed(*aircraft);

        // Simulate based on direction (arrival or departure)
        if (aircraft->direction == NORTH || aircraft->direction == SOUTH)
            simulateArrival(*aircraft, runways);
        else
            simulateDeparture(*aircraft, runways);

        if (aircraft->isFaulty)
            removeAircraftFromEverywhere(aircraft, activeAircrafts, runways);
        else
            ++it;
    }

    // Handle random ground faults every 30 seconds
    if (globalElapsedTime % 30 == 0)
        handleGroundFaults(activeAircrafts);

    // Check for aircrafts to be removed
    if (globalElapsedTime % 7 == 0 && globalElapsedTime != 0) {
        for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) {
            if ((*it)->phase == AT_GATE_BRS || (*it)->phase == DEPARTURE_CRUISE_BRS)
                removeAircraftFromEverywhere(*it, activeAircrafts, runways);
            else
                ++it;
        }
    }

    // Check for violations
    for (auto aircraft : activeAircrafts) {
        checkForViolations(*aircraft);
    }

    // Update time in flight for each aircraft
    for (auto& aircraft : activeAircrafts)
        aircraft->timeInFlight++;
}

bool kbhit() {
    char c;
    return read(STDIN_FILENO, &c, 1) == 1 && c == 'q';
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

Aircraft* createAircraftForManualEntry(vector<Airline>& airlines, Direction direction, int seq, string flightNum, AircraftType type, Priority priority, time_t schedTime) {
    Aircraft* aircraft = new Aircraft();

    // Select airline based on type
    Airline* selectedAirline = nullptr;
    for (auto& airline : airlines) {
        if (airline.type == type) {
            selectedAirline = &airline;
            break;
        }
    }
    if (!selectedAirline) selectedAirline = &airlines[0]; // Fallback

    aircraft->flightNumber = flightNum;
    aircraft->airline = selectedAirline;
    aircraft->type = selectedAirline->type;
    aircraft->direction = direction;
    aircraft->isEmergency = (priority == EMERGENCY_PRIORITY);
    aircraft->hasAVN = false;
    aircraft->isFaulty = false;
    aircraft->fuelStatus = 20 + rand() % 60; // Random fuel status between 20-80%
    aircraft->scheduledTime = schedTime;
    aircraft->priority = priority;

    // Set initial phase based on direction
    if (direction == NORTH || direction == SOUTH) {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        arrivalQueue.push(aircraft);
    } else {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        departureQueue.push(aircraft);
    }

    // Add to status map
    aircraftStatusMap[aircraft->flightNumber] = aircraft;

    return aircraft;
}

Aircraft* createAircraftForAutoEntry(const vector<Airline>& airlines, Direction direction, int seq) {
    Aircraft* aircraft = new Aircraft();

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
    for (const auto& airline : airlines) {
        if (isEmergency && (airline.type == EMERGENCY))
            emergencyEligibleAirlines.push_back(&airline);
        else if (!isEmergency && (airline.type != EMERGENCY))
            emergencyEligibleAirlines.push_back(&airline);
    }

    vector<const Airline*> eligibleAirlines;
    if (!isEmergency) {
        for (const auto& airline : emergencyEligibleAirlines) {
            if (cargoCreated && airline->type == CARGO)
                continue;
            else
                eligibleAirlines.push_back(airline);
        }
    }

    if (eligibleAirlines.empty()) {
        eligibleAirlines.push_back(&airlines[0]);
    }

    const Airline* selectedAirline = eligibleAirlines[rand() % eligibleAirlines.size()];
    if (selectedAirline->type == CARGO)
        cargoCreated = true;

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
    } else if (aircraft->type == CARGO || aircraft->fuelStatus < 30) {
        aircraft->priority = HIGH_PRIORITY;
    } else {
        aircraft->priority = NORMAL_PRIORITY;
    }

    // Set initial phase based on direction
    if (direction == NORTH || direction == SOUTH) {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        arrivalQueue.push(aircraft);
    } else {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        departureQueue.push(aircraft);
    }

    // Add to status map
    aircraftStatusMap[aircraft->flightNumber] = aircraft;

    return aircraft;
}

void simulateArrival(Aircraft& aircraft, vector<Runway>& runways) {
    switch (aircraft.phase) {
        case HOLDING:
            if (aircraft.timeInFlight >= 5) {
                aircraft.phase = APPROACH;
                aircraft.status = "Approaching";
                aircraft.fuelStatus -= 2;
            }
            break;

        case APPROACH: {
            Runway* assignedRunway = assignRunway(aircraft, runways);
            if (assignedRunway && aircraft.timeInFlight >= 9) {
                aircraft.phase = LANDING;
                aircraft.status = "Landing";
                aircraft.fuelStatus -= 2;
            }
            break;
        }

        case LANDING:
            if (aircraft.timeInFlight >= 14) {
                aircraft.phase = TAXI;
                aircraft.status = "Taxiing";
                aircraft.fuelStatus -= 2;

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
                aircraft.fuelStatus -= 2;
            }
            break;

        case AT_GATE:
            if (aircraft.timeInFlight >= 20 && aircraft.speed <= 0) {
                aircraft.phase = AT_GATE_BRS;
                aircraft.status = "Completed";
                aircraft.fuelStatus -= 1;
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
                aircraft.fuelStatus -= 2;
            }
            break;

        case TAXI: {
            Runway* assignedRunway = assignRunway(aircraft, runways);
            if (assignedRunway && aircraft.timeInFlight >= 6) {
                aircraft.phase = TAKEOFF_ROLL;
                aircraft.status = "Taking Off";
                aircraft.fuelStatus -= 2;
            }
            break;
        }

        case TAKEOFF_ROLL:
            if (aircraft.timeInFlight >= 12) {
                aircraft.phase = CLIMB;
                aircraft.status = "Climbing";
                aircraft.fuelStatus -= 2;

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
                aircraft.fuelStatus -= 2;
            }
            break;

        case ACCELERATING_TO_CRUISE:
            if (aircraft.timeInFlight >= 23) {
                aircraft.phase = DEPARTURE_CRUISE;
                aircraft.status = "Cruising";
                aircraft.fuelStatus -= 3;
            }
            break;

        case DEPARTURE_CRUISE:
            if (aircraft.timeInFlight >= 25) {
                aircraft.phase = DEPARTURE_CRUISE_BRS;
                aircraft.status = "Completed";
                aircraft.fuelStatus -= 4;
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
    // Check if the aircraft is in a waiting queue
    for (auto& runway : runways) {
        queue<Aircraft*> tempQueue = runway.waitingQueue;
        while (!tempQueue.empty()) {
            Aircraft* queuedAircraft = tempQueue.front();
            tempQueue.pop();
            if (queuedAircraft == &aircraft)
                return &runway;
        }
    }

    // Assign runway based on aircraft type and direction
    Runway* assignedRunway = nullptr;
    if (aircraft.priority == EMERGENCY_PRIORITY) {
        if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
        else if (!runways[RWY_A].isOccupied) assignedRunway = &runways[RWY_A];
        else if (!runways[RWY_B].isOccupied) assignedRunway = &runways[RWY_B];
        else {
            assignedRunway = &runways[RWY_C];
            for (auto& runway : runways) {
                if (runway.waitingQueue.size() < assignedRunway->waitingQueue.size())
                    assignedRunway = &runway;
            }
            assignedRunway->waitingQueue.push(&aircraft);
            aircraft.status = "Waiting for Runway";
            return nullptr;
        }
        assignedRunway->isOccupied = true;
        assignedRunway->currentAircraft = &aircraft;
        return assignedRunway;
    }

    if (aircraft.type == CARGO) {
        if (!runways[RWY_C].isOccupied) {
            assignedRunway = &runways[RWY_C];
            assignedRunway->isOccupied = true;
            assignedRunway->currentAircraft = &aircraft;
            return assignedRunway;
        } else {
            runways[RWY_C].waitingQueue.push(&aircraft);
            aircraft.status = "Waiting for Runway";
            return nullptr;
        }
    }

    if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
        if (!runways[RWY_A].isOccupied) assignedRunway = &runways[RWY_A];
        else if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
        else {
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
    } else {
        if (!runways[RWY_B].isOccupied) assignedRunway = &runways[RWY_B];
        else if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
        else {
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

int calculateSpeedChange(const Aircraft& aircraft) {
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

void updateAircraftSpeed(Aircraft& aircraft) {
    int speedChange = calculateSpeedChange(aircraft);
    if (aircraft.direction == NORTH || aircraft.direction == SOUTH)
        aircraft.speed -= speedChange;
    else
        aircraft.speed += speedChange;

    if (aircraft.speed < 0) {
        if (aircraft.phase != AT_GATE)
            aircraft.speed = (rand() % 10) + 10;
        else
            aircraft.speed = 0;
    }
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
        if ((aircraft->phase == TAXI || aircraft->phase == AT_GATE) && !aircraft->isFaulty) {
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

    int minutes = elapsedTime / 60;
    int seconds = elapsedTime % 60;

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

        if (!runway.waitingQueue.empty()) {
            cout << " | Waiting: " << runway.waitingQueue.size() << " aircraft";
        }
        cout << endl;
    }

    cout << "\nArrival Queue: " << arrivalQueue.size() << " aircraft" << endl;
    cout << "Departure Queue: " << departureQueue.size() << " aircraft" << endl;

    cout << "\nActive Aircraft (" << aircrafts.size() << "):" << endl;
    for (const auto& aircraft : aircrafts) {
        if (aircraft->isFaulty) continue;

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

void removeAircraftFromEverywhere(Aircraft* aircraft, vector<Aircraft*>& aircrafts, vector<Runway>& runways) {
    auto it = find(aircrafts.begin(), aircrafts.end(), aircraft);
    if (it != aircrafts.end()) {
        aircrafts.erase(it);
    }

    for (auto& runway : runways) {
        if (runway.currentAircraft == aircraft) {
            runway.isOccupied = false;
            runway.currentAircraft = nullptr;
        } else {
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
    }

    aircraftStatusMap.erase(aircraft->flightNumber);

    if (aircraft->direction == NORTH || aircraft->direction == SOUTH) {
        priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> tempQueue;
        while (!arrivalQueue.empty()) {
            Aircraft* queuedAircraft = arrivalQueue.top();
            arrivalQueue.pop();
            if (queuedAircraft != aircraft)
                tempQueue.push(queuedAircraft);
        }
        arrivalQueue = tempQueue;
    } else {
        priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> tempQueue;
        while (!departureQueue.empty()) {
            Aircraft* queuedAircraft = departureQueue.top();
            departureQueue.pop();
            if (queuedAircraft != aircraft)
                tempQueue.push(queuedAircraft);
        }
        departureQueue = tempQueue;
    }

    delete aircraft;
}