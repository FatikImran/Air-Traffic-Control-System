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
#include <regex>
#include <limits>

using namespace std;

// Constants
const int SIMULATION_DURATION = 300;        // 5 minutes in seconds
const int TIME_STEP = 1;                   // 1 second per simulation step
const string ADMIN_USERNAME = "admin";
const string ADMIN_PASSWORD = "atcs2025";
const int AGING_THRESHOLD = 30;            // Threshold for priority upgrade due to waiting
const int AVG_RUNWAY_USAGE_TIME = 5;       // Average time (seconds) for runway usage (landing/takeoff)

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
    int actualWaitTime = 0;
    int estimatedWaitTime = 0;
    string status = "Waiting";
    double altitude = 0; // feet
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

// Input validation helper functions
// Clear input buffer to prevent residual input issues
void clearInputBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

// Validate if string is a valid integer within range
bool isValidInteger(const string& str, int& value, int min, int max) {
    try {
        size_t pos;
        value = stoi(str, &pos);
        if (pos != str.length()) return false; // Ensure entire string is consumed
        return value >= min && value <= max;
    } catch (...) {
        return false;
    }
}

// Validate time format (YYYY-MM-DD HH:MM) and ranges
bool isValidTimeFormat(const string& timeStr, struct tm& tm) {
    // Check format with regex
    regex timeRegex("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}");
    if (!regex_match(timeStr, timeRegex)) {
        return false;
    }

    // Parse time
    istringstream ss(timeStr);
    ss >> get_time(&tm, "%Y-%m-%d %H:%M");
    if (ss.fail()) {
        return false;
    }

    // Validate ranges
    if (tm.tm_year < 125) { // Year >= 2025 (tm_year is years since 1900)
        return false;
    }
    if (tm.tm_mon < 0 || tm.tm_mon > 11) { // Months 0-11
        return false;
    }
    if (tm.tm_hour < 0 || tm.tm_hour > 23) {
        return false;
    }
    if (tm.tm_min < 0 || tm.tm_min > 59) {
        return false;
    }

    // Validate day of month
    int daysInMonth[] = {31, (tm.tm_year % 4 == 0 && tm.tm_year % 100 != 0) || (tm.tm_year % 400 == 0) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (tm.tm_mday < 1 || tm.tm_mday > daysInMonth[tm.tm_mon]) {
        return false;
    }

    return true;
}

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
void updateAircraftAltitude(Aircraft& aircraft);
double calculateAltitudeChange(const Aircraft& aircraft);
void checkForViolations(Aircraft& aircraft);
void displayStatus(const vector<Aircraft*>& aircrafts, const vector<Runway>& runways, int elapsedTime);
void handleGroundFaults(vector<Aircraft*>& aircrafts);
Runway* assignRunway(Aircraft& aircraft, vector<Runway>& runways);
string getPhaseName(FlightPhase phase);
string getDirectionName(Direction dir);
string getAircraftTypeName(AircraftType type);
void updateWaitTimes(vector<Runway>& runways);
void updateAircraftStatus(Aircraft& aircraft);
bool kbhit();
void updateSimulationStep(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts);
void rebuildQueue(Aircraft* aircraft, priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue);
bool isAircraftWaiting(const Aircraft& aircraft, const vector<Runway>& runways);
int calculateEstimatedWaitTime(Aircraft& aircraft, const vector<Runway>& runways);
void displayAVNStatistics(const vector<Airline>& airlines);

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
        string input;
        // Validate menu choice
        while (true) {
            cout << "Enter your choice: ";
            getline(cin, input);
            if (isValidInteger(input, choice, 1, 6)) {
                break;
            }
            cout << "Invalid input. Please enter a number between 1 and 6. Press Enter to continue...\n";
            clearInputBuffer();
        }

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
                    // Display AVN statistics before cleanup
                    displayAVNStatistics(airlines);
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
                    showSimulation(airlines, runways, activeAircrafts, aircraftSequence);
                } else {
                    cout << "No simulation is running!\n";
                }
                break;
            case 6: // Exit
                if (simulationRunning) {
                    lock_guard<mutex> guard(simulationMutex);
                    simulationRunning = false;
                    simulationPaused = true;
                    // Display AVN statistics before cleanup
                    displayAVNStatistics(airlines);
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
    ║        █████╗ ██╗  ██╗ ███████╗     █████╗ ████████╗ ██████╗ ███████╗    ║
    ║       ██╔══██╗██║ ██╔╝ ██╔════╝    ██╔══██╗╚══██╔══╝██╔════╝ ██╔════╝     ║
    ║      ███████║█████╔╝   █████╗      ███████║   ██║   ██║       ███████╗    ║
    ║      ██╔══██║██╔═██╗   ██╔══╝      ██╔══██║   ██║   ██║   ██║╚════██║     ║
    ║      ██║  ██║██║  ██╗  ██║         ██║  ██║   ██║   ╚██████╔╝███████║    ║
    ║      ╚═╝  ╚═╝╚═╝  ╚═╝  ╚═╝       ╚═╝  ╚═╝   ╚═╝    ╚═════╝ ╚══════╝      ║
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
        // Validate username
        while (true) {
            cout << "Username: ";
            getline(cin, username);
            if (!username.empty() && username.length() <= 50) {
                break;
            }
            cout << "Invalid username. Must be non-empty and less than 50 characters. Press Enter to continue...\n";
            clearInputBuffer();
        }

        // Validate password
        while (true) {
            cout << "Password: ";
            getline(cin, password);
            if (!password.empty() && password.length() <= 50) {
                break;
            }
            cout << "Invalid password. Must be non-empty and less than 50 characters. Press Enter to continue...\n";
            clearInputBuffer();
        }

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
	string input;
    Airline* selectedAirline = nullptr;
    bool validAirline = false;
    while (!validAirline) {
        cout << "Select airline (1-" << airlines.size() << "): ";
        getline(cin, input);

        if (!isValidInteger(input, airlineChoice, 1, airlines.size())) {
            cout << "Invalid airline selection. Please try again. Press Enter to continue...\n";
			clearInputBuffer();
            continue;
        }
        selectedAirline = &airlines[airlineChoice - 1];
        if (selectedAirline->type == CARGO && cargoCreated) {
            cout << "Error: Only one cargo flight is allowed per simulation. Please select a different airline.\n";
        } else {
            validAirline = true;
        }
    }

    // Get flight number
    string flightNumber;
    while (true) {
        cout << "Enter flight number (e.g., PK101): ";
        getline(cin, flightNumber);
        if (flightNumber.empty() || flightNumber.length() > 10) {
            cout << "Invalid flight number. Must be non-empty and less than 10 characters. Press Enter to continue...\n";
            clearInputBuffer();
            continue;
        }
        // Check if flight number is unique
        bool isUnique = true;
        for (const auto* aircraft : activeAircrafts) {
            if (aircraft->flightNumber == flightNumber) {
                isUnique = false;
                break;
            }
        }
        if (!isUnique) {
            cout << "Error: Flight number " << flightNumber << " is already in use. Please enter a unique flight number. Press Enter to continue...\n";
            clearInputBuffer();
            continue;
        }
        break;
    }

    // Get direction
    int dirChoice;
    while (true) {
        cout << "Select direction (1. North, 2. South, 3. East, 4. West): ";
        getline(cin, input);
        if (isValidInteger(input, dirChoice, 1, 4)) {
            break;
        }
        cout << "Invalid input. Please enter a number between 1 and 4. Press Enter to continue...\n";
        clearInputBuffer();
    }
    Direction direction;
    switch (dirChoice) {
        case 1: direction = NORTH; break;
        case 2: direction = SOUTH; break;
        case 3: direction = EAST; break;
        case 4: direction = WEST; break;
    }

    // Get priority
    int priChoice;
    while (true) {
        cout << "Select priority (1. Emergency, 2. High, 3. Normal): ";
        getline(cin, input);
        if (isValidInteger(input, priChoice, 1, 3)) {
            break;
        }
        cout << "Invalid input. Please enter a number between 1 and 3. Press Enter to continue...\n";
        clearInputBuffer();
    }
    Priority priority;
    switch (priChoice) {
        case 1: priority = EMERGENCY_PRIORITY; break;
        case 2: priority = HIGH_PRIORITY; break;
        case 3: priority = NORMAL_PRIORITY; break;
    }

    // Get scheduled time
    string schedTimeStr;
    struct tm tm = {};
    time_t scheduledTime;
    while (true) {
        cout << "Enter scheduled time (YYYY-MM-DD HH:MM) or 'now' for current time: ";
        getline(cin, schedTimeStr);
        // Check for NOW (case-insensitive)
        string lowerInput = schedTimeStr;
        transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        if (lowerInput == "now") {
            scheduledTime = time(nullptr);
            break;
        }
        // Validate time format
        if (isValidTimeFormat(schedTimeStr, tm)) {
            scheduledTime = mktime(&tm);
            break;
        }
        cout << "Invalid input. Use YYYY-MM-DD HH:MM with valid ranges (e.g., year >= 2025, hours 0-23) or 'now'.\nPress Enter to continue...\n";
        clearInputBuffer();
    }

    // Create aircraft
    Aircraft* newAircraft = createAircraftForManualEntry(airlines, direction, ++aircraftSequence, flightNumber, selectedAirline->type, priority, scheduledTime);
    activeAircrafts.push_back(newAircraft);

    // Update cargoCreated if a cargo aircraft was spawned
    if (selectedAirline->type == CARGO) {
        cargoCreated = true;
    }

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
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    lock_guard<mutex> guard(simulationMutex);
    if (simulationRunning) {
        simulationRunning = false;
        simulationPaused = true;
        cout << "\nSimulation completed.\n";
        displayAVNStatistics(airlines);
    }
}

void showSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    setNonBlockingInput(true);
    {
        lock_guard<mutex> guard(coutMutex);
        cout << "Showing simulation. Press 'q' to return to menu.\n" << flush;
    }

    auto startTime = chrono::steady_clock::now();
    while (simulationRunning && globalElapsedTime < SIMULATION_DURATION) {
        {
            lock_guard<mutex> guard(simulationMutex);
            if (kbhit()) {
                cout << "Returning to menu.\n" << flush;
                simulationPaused = true;
                break;
            }
            simulationPaused = false;
            displayStatus(activeAircrafts, runways, globalElapsedTime);
            updateSimulationStep(airlines, runways, activeAircrafts);
            globalElapsedTime += TIME_STEP;
        }
        this_thread::sleep_for(chrono::milliseconds(TIME_STEP * 1000));

        // Timeout after 30 seconds to prevent freezing
        auto elapsed = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime).count();
        if (elapsed >= 30) {
            lock_guard<mutex> guard(coutMutex);
            cout << "Timeout: Returning to menu.\n" << flush;
            lock_guard<mutex> guard2(simulationMutex);
            simulationPaused = true;
            break;
        }
    }

    setNonBlockingInput(false);
}

void updateSimulationStep(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts) {
    updateWaitTimes(runways);

    // Process each aircraft
    for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) {
        Aircraft* aircraft = *it;

        // Update aircraft status
        updateAircraftStatus(*aircraft);

        // Update aircraft speed and check for violations
        updateAircraftSpeed(*aircraft);
		updateAircraftAltitude(*aircraft);

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
    for (auto& aircraft : activeAircrafts) {
        if (!isAircraftWaiting(*aircraft, runways)) // Only update if not waiting
            aircraft->timeInFlight++;
    }
}

bool kbhit() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        lock_guard<mutex> guard(coutMutex);
        cout << "Key detected: " << c << "\n" << flush; // Debug keypress
        return c == 'q';
    }
    return false;
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

    // Set initial phase, speed, and altitude based on direction
    if (direction == NORTH || direction == SOUTH) {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        aircraft->altitude = 19000 + (rand() % 1000); // 19,000-20,000 ft
        arrivalQueue.push(aircraft);
    } else {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        aircraft->altitude = 0; // On ground
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

    // Set initial phase, speed, and altitude based on direction
    if (direction == NORTH || direction == SOUTH) {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        aircraft->altitude = 10000 + (rand() % 10001); // 10,000-20,000 ft
        arrivalQueue.push(aircraft);
        // Rebuild queue if priority is HIGH_PRIORITY due to low fuel
        if (aircraft->fuelStatus < 30 && aircraft->priority == HIGH_PRIORITY) {
            cout << "Flight " << aircraft->flightNumber << " upgraded to HIGH_PRIORITY due to low fuel.\n";
            rebuildQueue(aircraft, arrivalQueue);
        }
    } else {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        aircraft->altitude = 0; // On ground
        departureQueue.push(aircraft);
        // Rebuild queue if priority is HIGH_PRIORITY due to low fuel
        if (aircraft->fuelStatus < 30 && aircraft->priority == HIGH_PRIORITY) {
            cout << "Flight " << aircraft->flightNumber << " upgraded to HIGH_PRIORITY due to low fuel.\n";
            rebuildQueue(aircraft, departureQueue);
        }
    }

    // Add to status map
    aircraftStatusMap[aircraft->flightNumber] = aircraft;

    return aircraft;
}

void simulateArrival(Aircraft& aircraft, vector<Runway>& runways) {
    // Skip phase transitions if aircraft is waiting for a runway
    if (isAircraftWaiting(aircraft, runways)) {
        return;
    }

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
    // Skip phase transitions if aircraft is waiting for a runway
    if (isAircraftWaiting(aircraft, runways)) {
        return;
    }

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

void updateWaitTimes(vector<Runway>& runways) {
    for (auto& pair : aircraftStatusMap) {
        Aircraft* aircraft = pair.second;
        // Update actual wait time for aircraft in waiting queue
        if (aircraft->status == "Waiting for Runway") {
            aircraft->actualWaitTime++;
            // Aging: Upgrade NORMAL_PRIORITY to HIGH_PRIORITY after AGING_THRESHOLD
            if (aircraft->actualWaitTime >= AGING_THRESHOLD && aircraft->priority == NORMAL_PRIORITY) {
                aircraft->priority = HIGH_PRIORITY;
                cout << "Flight " << aircraft->flightNumber << " upgraded to HIGH_PRIORITY due to aging.\n";
                // Rebuild queue
                priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue = (aircraft->direction == NORTH || aircraft->direction == SOUTH) ? arrivalQueue : departureQueue;
                rebuildQueue(aircraft, queue);
            }
        }
        // Calculate estimated wait time for all aircraft
        aircraft->estimatedWaitTime = calculateEstimatedWaitTime(*aircraft, runways);
    }
}

int calculateEstimatedWaitTime(Aircraft& aircraft, const vector<Runway>& runways) {
    // If aircraft is past runway phases, no wait time
    if ((aircraft.direction == NORTH || aircraft.direction == SOUTH) &&
        (aircraft.phase == LANDING || aircraft.phase == TAXI || aircraft.phase == AT_GATE || aircraft.phase == AT_GATE_BRS)) {
        return 0;
    }
    if ((aircraft.direction == EAST || aircraft.direction == WEST) &&
        (aircraft.phase == TAKEOFF_ROLL || aircraft.phase == CLIMB || aircraft.phase == ACCELERATING_TO_CRUISE ||
         aircraft.phase == DEPARTURE_CRUISE || aircraft.phase == DEPARTURE_CRUISE_BRS)) {
        return 0;
    }

    int estimatedWait = 0;

    // Check if aircraft is in a waiting queue
    if (isAircraftWaiting(aircraft, runways)) {
        // Find the runway and queue position
        for (const auto& runway : runways) {
            queue<Aircraft*> tempQueue = runway.waitingQueue;
            int position = 0;
            bool found = false;
            vector<Aircraft*> higherOrEqualPriority;

            // Collect aircraft in queue
            while (!tempQueue.empty()) {
                Aircraft* queuedAircraft = tempQueue.front();
                tempQueue.pop();
                if (queuedAircraft == &aircraft) {
                    found = true;
                }
                if (queuedAircraft->priority <= aircraft.priority) { // Higher or equal priority
                    higherOrEqualPriority.push_back(queuedAircraft);
                }
            }

            if (found) {
                // Sort by scheduled time for FCFS within priority
                sort(higherOrEqualPriority.begin(), higherOrEqualPriority.end(),
                     [](Aircraft* a, Aircraft* b) { return a->scheduledTime < b->scheduledTime; });

                // Find position of current aircraft
                for (size_t i = 0; i < higherOrEqualPriority.size(); ++i) {
                    if (higherOrEqualPriority[i] == &aircraft) {
                        position = i;
                        break;
                    }
                }

                // Calculate wait time: position * average runway usage + current aircraft time
                estimatedWait = position * AVG_RUNWAY_USAGE_TIME;
                if (runway.isOccupied && runway.currentAircraft) {
                    // Add remaining time for current aircraft (approximate as full runway usage)
                    estimatedWait += AVG_RUNWAY_USAGE_TIME;
                }
                break;
            }
        }
    } else {
        // Estimate time to reach runway-requiring phase
        if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
            // Arrivals
            if (aircraft.phase == HOLDING) {
                estimatedWait = 9; // 5s (HOLDING) + 4s (APPROACH to runway)
            } else if (aircraft.phase == APPROACH) {
                estimatedWait = 4; // 4s (APPROACH to runway)
            }
        } else {
            // Departures
            if (aircraft.phase == AT_GATE) {
                estimatedWait = 6; // 3s (AT_GATE) + 3s (TAXI to runway)
            } else if (aircraft.phase == TAXI) {
                estimatedWait = 3; // 3s (TAXI to runway)
            }
        }

        // Add potential queue wait time
        priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> queueCopy =
            (aircraft.direction == NORTH || aircraft.direction == SOUTH) ? arrivalQueue : departureQueue;
        int aheadCount = 0;

        // Count aircraft with higher or equal priority
        while (!queueCopy.empty()) {
            Aircraft* queuedAircraft = queueCopy.top();
            queueCopy.pop();
            if (queuedAircraft != &aircraft && queuedAircraft->priority <= aircraft.priority) {
                aheadCount++;
            }
        }

        // Add queue wait time
        estimatedWait += aheadCount * AVG_RUNWAY_USAGE_TIME;

        // Add time for currently occupying aircraft
        for (const auto& runway : runways) {
            if (runway.isOccupied && runway.currentAircraft &&
                ((aircraft.direction == NORTH || aircraft.direction == SOUTH) &&
                 (runway.id == RWY_A || runway.id == RWY_C)) ||
                ((aircraft.direction == EAST || aircraft.direction == WEST) &&
                 (runway.id == RWY_B || runway.id == RWY_C))) {
                estimatedWait += AVG_RUNWAY_USAGE_TIME;
            }
        }
    }

    return estimatedWait;
}

void updateAircraftStatus(Aircraft& aircraft) {
    // Skip status update if aircraft is waiting for a runway
    if (aircraft.status == "Waiting for Runway") {
        return;
    }

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

double calculateAltitudeChange(const Aircraft& aircraft) {
    double altitudeChange;

    switch (aircraft.phase) {
        case HOLDING:
            altitudeChange = 1800 + rand() % 200;
            break;
        case APPROACH:
            altitudeChange = 1700 + rand() % 50;
            break;
        case LANDING:
            altitudeChange = 500 + rand() % 100;
            break;
        case TAXI:
        case AT_GATE:
        case AT_GATE_BRS:
            altitudeChange = 0;
            break;
        case TAKEOFF_ROLL:
            altitudeChange = 150 + rand() % 30;
            break;
        case CLIMB:
            altitudeChange = 4600 + rand() % 250;
            break;
        case ACCELERATING_TO_CRUISE:
            altitudeChange = 1400 + rand() % 25; 
            break;
        case DEPARTURE_CRUISE:
        case DEPARTURE_CRUISE_BRS:
            altitudeChange = 3000 + rand() % 2000;
            break;
        default:
            altitudeChange = 0;
            break;
    }

    return altitudeChange;
}

void updateAircraftSpeed(Aircraft& aircraft) {
    // If aircraft is waiting for a runway, set fixed speed and skip updates
    if (aircraft.status == "Waiting for Runway") {
        if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
            // Arrivals: Maintain APPROACH speed (240-290 km/h)
            aircraft.speed = 240 + rand() % 51; // Random speed in 240-290 km/h
        } else {
            // Departures: Set speed to 0 (stationary while waiting)
            aircraft.speed = 0;
        }
        return;
    }

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

void updateAircraftAltitude(Aircraft& aircraft) {
    // If aircraft is waiting for a runway, set fixed or constrained altitude and skip updates
    if (aircraft.status == "Waiting for Runway") {
        if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
            // Arrivals: Maintain HOLDING altitude (10,000-20,000 ft)
            double altitudeChange = (rand() % 101 - 50); // -50 to +50 ft/s
            aircraft.altitude += altitudeChange;
        } else {
            // Departures: Set altitude to 0 (on ground)
            aircraft.altitude = 0;
        }
        return;
    }

    double altitudeChange = calculateAltitudeChange(aircraft);
    if (aircraft.direction == NORTH || aircraft.direction == SOUTH)
        aircraft.altitude += altitudeChange; // Negative for descent
    else
        aircraft.altitude += altitudeChange; // Positive for climb

    // Enforce phase-specific altitude ranges
    switch (aircraft.phase) {
        case HOLDING:
            aircraft.altitude = max(10000.0, min(20000.0, aircraft.altitude));
            break;
        case APPROACH:
            aircraft.altitude = max(3000.0, min(10000.0, aircraft.altitude));
            break;
        case LANDING:
            aircraft.altitude = max(0.0, min(3000.0, aircraft.altitude));
            break;
        case TAXI:
        case AT_GATE:
        case AT_GATE_BRS:
            aircraft.altitude = 0;
            break;
        case TAKEOFF_ROLL:
            aircraft.altitude = max(0.0, min(1000.0, aircraft.altitude));
            break;
        case CLIMB:
            aircraft.altitude = max(1000.0, min(20000.0, aircraft.altitude));
            break;
        case ACCELERATING_TO_CRUISE:
            aircraft.altitude = max(20000.0, min(30000.0, aircraft.altitude));
            break;
        case DEPARTURE_CRUISE:
        case DEPARTURE_CRUISE_BRS:
            aircraft.altitude = max(30000.0, min(40000.0, aircraft.altitude));
            break;
        default:
            break;
    }

    // Ensure altitude is non-negative
    if (aircraft.altitude < 0) {
        aircraft.altitude = 0;
    }
}

void checkForViolations(Aircraft& aircraft) {
    bool speedViolation = false;
    bool altitudeViolation = false;

    // Speed and altitude checks
    switch (aircraft.phase) {
        case HOLDING:
            speedViolation = (aircraft.speed > 600);
            altitudeViolation = (aircraft.altitude < 10000 || aircraft.altitude > 20000);
            break;
        case APPROACH:
            speedViolation = (aircraft.speed < 240 || aircraft.speed > 290);
            altitudeViolation = (aircraft.altitude < 3000 || aircraft.altitude > 10000);
            break;
        case LANDING:
            speedViolation = (aircraft.speed > 240);
            altitudeViolation = (aircraft.altitude > 3000);
            break;
        case TAXI:
            speedViolation = (aircraft.speed > 30);
            altitudeViolation = (aircraft.altitude != 0);
            break;
        case AT_GATE:
            speedViolation = (aircraft.speed > 10);
            altitudeViolation = (aircraft.altitude != 0);
            break;
        case TAKEOFF_ROLL:
            speedViolation = (aircraft.speed > 290);
            altitudeViolation = (aircraft.altitude > 1000);
            break;
        case CLIMB:
            speedViolation = (aircraft.speed > 463);
            altitudeViolation = (aircraft.altitude < 1000 || aircraft.altitude > 20000);
            break;
        case ACCELERATING_TO_CRUISE:
            speedViolation = false;
            altitudeViolation = false;
            break;
        case DEPARTURE_CRUISE:
            speedViolation = (aircraft.speed < 800 || aircraft.speed > 900);
            altitudeViolation = (aircraft.altitude < 30000 || aircraft.altitude > 40000);
            break;
        default:
            break;
    }

    if ((speedViolation || altitudeViolation) && !aircraft.hasAVN) {
        aircraft.hasAVN = true;
        aircraft.airline->violations++;

        lock_guard<mutex> guard(coutMutex);
        cout << "VIOLATION: " << aircraft.flightNumber << " (" << aircraft.airline->name << ") in "
             << getPhaseName(aircraft.phase) << " phase - ";
        if (speedViolation) {
            cout << "Speed: " << aircraft.speed << " km/h (limits: ";
            switch (aircraft.phase) {
                case HOLDING: cout << "≤600"; break;
                case APPROACH: cout << "240-290"; break;
                case LANDING: cout << "≤240"; break;
                case TAXI: cout << "≤30"; break;
                case AT_GATE: cout << "≤10"; break;
                case TAKEOFF_ROLL: cout << "≤290"; break;
                case CLIMB: cout << "≤463"; break;
                case DEPARTURE_CRUISE: cout << "800-900"; break;
                case AT_GATE_BRS: cout << "≤10"; break;
                case DEPARTURE_CRUISE_BRS: cout << "800-900"; break;
                default: cout << "N/A"; break;
            }
            cout << " km/h)";
        }
        if (speedViolation && altitudeViolation) cout << ", ";
        if (altitudeViolation) {
            cout << "Altitude: " << aircraft.altitude << " ft (limits: ";
            switch (aircraft.phase) {
                case HOLDING: cout << "10000-20000"; break;
                case APPROACH: cout << "3000-10000"; break;
                case LANDING: cout << "0-3000"; break;
                case TAXI: cout << "0"; break;
                case AT_GATE: cout << "0"; break;
                case TAKEOFF_ROLL: cout << "0-1000"; break;
                case CLIMB: cout << "1000-20000"; break;
                case ACCELERATING_TO_CRUISE: cout << "20000-30000"; break;
                case DEPARTURE_CRUISE: cout << "30000-40000"; break;
                case AT_GATE_BRS: cout << "0"; break;
                case DEPARTURE_CRUISE_BRS: cout << "30000-40000"; break;
                default: cout << "N/A"; break;
            }
            cout << " ft)";
        }
        cout << endl;
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
             << ", Altitude: " << aircraft->altitude << " ft"
             << ", Fuel: " << aircraft->fuelStatus << "%"
             << ", Priority: ";

        switch (aircraft->priority) {
            case EMERGENCY_PRIORITY: cout << "Emergency"; break;
            case HIGH_PRIORITY: cout << "High"; break;
            case NORMAL_PRIORITY: cout << "Normal"; break;
        }

        // Display estimated wait time for all aircraft
        cout << ", Est. Wait: " << aircraft->estimatedWaitTime << "s";

        // Display actual wait time for aircraft in waiting queue
        if (aircraft->status == "Waiting for Runway") {
            cout << ", Actual Wait: " << aircraft->actualWaitTime << "s";
        }

        cout << (aircraft->hasAVN ? " [AVN ISSUED]" : "") << endl;
    }

    cout << setfill(' ') << "==================================" << endl << flush;
}

void displayAVNStatistics(const vector<Airline>& airlines) {
    lock_guard<mutex> guard(coutMutex);
    
    // Column widths
    const int nameWidth = 25;
    const int typeWidth = 12;
    const int violationsWidth = 12;

    // Calculate total violations
    int totalViolations = 0;
    for (const auto& airline : airlines) {
        totalViolations += airline.violations;
    }

    // Header
    cout << setfill(' ') << "\n=============================\n";
    cout << "| AirControlX AVN Statistics |\n";
    cout << "=============================\n";

    // Table header
    cout << "| " << left << setw(nameWidth) << "Airline Name" 
         << "| " << left << setw(typeWidth) << "Type" 
         << "| " << right << setw(violationsWidth) << "Violations" << " |\n";
    cout << "+-" << string(nameWidth, '-') << "-+-" 
         << string(typeWidth, '-') << "-+-" 
         << string(violationsWidth, '-') << "-+\n";

    // Table rows
    for (const auto& airline : airlines) {
        cout << "| " << left << setw(nameWidth) << airline.name 
             << "| " << left << setw(typeWidth) << getAircraftTypeName(airline.type) 
             << "| " << right << setw(violationsWidth) << airline.violations << " |\n";
    }

    // Table footer
    cout << "+-" << string(nameWidth, '-') << "-+-" 
         << string(typeWidth, '-') << "-+-" 
         << string(violationsWidth, '-') << "-+\n";
    cout << "| Total Violations: " << left << setw(nameWidth + typeWidth - 16) << totalViolations 
         << "| " << right << setw(violationsWidth) << " " << " |\n";
    cout << "=============================\n\n" << flush;
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

void rebuildQueue(Aircraft* aircraft, priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue) {
    vector<Aircraft*> temp;
    bool found = false;
    while (!queue.empty()) {
        Aircraft* a = queue.top();
        queue.pop();
        if (a == aircraft) {
            found = true;
        } else {
            temp.push_back(a);
        }
    }
    if (found) {
        for (auto a : temp) {
            queue.push(a);
        }
        queue.push(aircraft);
    }
}

bool isAircraftWaiting(const Aircraft& aircraft, const vector<Runway>& runways) {
    for (const auto& runway : runways) {
        queue<Aircraft*> tempQueue = runway.waitingQueue;
        while (!tempQueue.empty()) {
            Aircraft* queuedAircraft = tempQueue.front();
            tempQueue.pop();
            if (queuedAircraft == &aircraft) {
                return true;
            }
        }
    }
    return false;
}