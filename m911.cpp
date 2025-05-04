/*

---------   MUHAMMAD FATIK BIN IMRAN (23i-0655)
---------   MUHAMMAD KALEEM AKHTAR (23i-0524)
-----   SECTION: BCS-4C
-----   AirControlX - Automated Air Traffic Control System
-----   Spring 2025 Operating Systems Project
*/

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <ctime>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <queue>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <limits>

using namespace std;

// Constants
const int SIMULATION_DURATION = 300;        // 5 minutes in seconds
const int TIME_STEP = 1;                   // 1 second per simulation step
const string ADMIN_USERNAME = "admin";
const string ADMIN_PASSWORD = "atcs2025";
const int AGING_THRESHOLD = 30;            // Seconds to upgrade NORMAL_PRIORITY to HIGH_PRIORITY

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

// Enum to string conversion functions
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
        case AT_GATE_BRS: return "At Gate (Completed)";
        case DEPARTURE_CRUISE_BRS: return "Departure Cruise (Completed)";
        case ACCELERATING_TO_CRUISE: return "Accelerating to Cruise";
        default: return "Unknown";
    }
}

// Forward declaration of Aircraft struct
struct Aircraft;

// Airline structure
struct Airline {
    string name;
    AircraftType type;
    int aircraftCount;
    int flightCount;
    int violations;
};

// Runway structure
struct Runway {
    RunwayID id;
    string name;
    bool isOccupied;
    Aircraft* currentAircraft;
    queue<Aircraft*> waitingQueue;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
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
    pthread_t thread; // Thread for this aircraft
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
pthread_mutex_t coutMutex;
pthread_mutex_t simulationMutex;
pthread_mutex_t arrivalQueueMutex;
pthread_mutex_t departureQueueMutex;
pthread_cond_t simulationCond;
bool cargoCreated = false;
priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> arrivalQueue;
priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> departureQueue;
map<string, Aircraft*> aircraftStatusMap;
bool simulationRunning = false;
bool simulationPaused = true; // Start paused until shown
int globalElapsedTime = 0; // Track simulation time globally
vector<Aircraft*> activeAircrafts; // For thread management
vector<Runway> runways; // Global runways vector
struct termios originalTermios; // Store original terminal state

// Function prototypes
void displayWelcomeScreen();
bool login();
void displayMenu();
void spawnCustomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void spawnRandomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void startSimulation(vector<Airline>& airlines, vector<Runway>& runways);
void showSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void removeAircraftFromEverywhere(Aircraft* aircraft, vector<Aircraft*>& aircrafts, vector<Runway>& runways);
void initializeAirlines(vector<Airline>& airlines);
void initializeRunways(vector<Runway>& runways);
string generateFlightNumber(const Airline& airline, int sequence);
Aircraft* createAircraftForManualEntry(vector<Airline>& airlines, Direction direction, int seq, string flightNum, AircraftType type, Priority priority, time_t schedTime);
Aircraft* createAircraftForAutoEntry(const vector<Airline>& airlines, Direction direction, int seq);
void* aircraftThreadFunc(void* arg);
void simulateArrival(Aircraft& aircraft, vector<Runway>& runways);
void simulateDeparture(Aircraft& aircraft, vector<Runway>& runways);
void updateAircraftSpeed(Aircraft& aircraft);
void checkForViolations(Aircraft& aircraft);
void displayStatus(const vector<Aircraft*>& aircrafts, const vector<Runway>& runways, int elapsedTime);
void handleGroundFaults(vector<Aircraft*>& aircrafts);
Runway* assignRunway(Aircraft& aircraft, vector<Runway>& runways);
void updateWaitTimes();
void updateAircraftStatus(Aircraft& aircraft);
bool kbhit();
void updateSimulationStep(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts);
void rebuildQueue(Aircraft* aircraft, priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue, pthread_mutex_t& queueMutex);
void initMutex(pthread_mutex_t& mutex, const char* name);
void initCond(pthread_cond_t& cond, const char* name);
void destroyMutex(pthread_mutex_t& mutex, const char* name);
void destroyCond(pthread_cond_t& cond, const char* name);
void setNonBlockingInput(bool enable);

// Initialize pthread mutex and condition variable with error handling
void initMutex(pthread_mutex_t& mutex, const char* name) {
    int ret = pthread_mutex_init(&mutex, nullptr);
    if (ret != 0) {
        cerr << "Failed to initialize " << name << ": " << strerror(ret) << endl;
        exit(1);
    }
}

void initCond(pthread_cond_t& cond, const char* name) {
    int ret = pthread_cond_init(&cond, nullptr);
    if (ret != 0) {
        cerr << "Failed to initialize " << name << ": " << strerror(ret) << endl;
        exit(1);
    }
}

// Destroy pthread mutex and condition variable
void destroyMutex(pthread_mutex_t& mutex, const char* name) {
    int ret = pthread_mutex_destroy(&mutex);
    if (ret != 0) {
        cerr << "Failed to destroy " << name << ": " << strerror(ret) << endl;
    }
}

void destroyCond(pthread_cond_t& cond, const char* name) {
    int ret = pthread_cond_destroy(&cond);
    if (ret != 0) {
        cerr << "Failed to destroy " << name << ": " << strerror(ret) << endl;
    }
}

// Set terminal to non-blocking or blocking input mode with state preservation
void setNonBlockingInput(bool enable) {
    static bool isNonBlocking = false; // Track current state
    static struct termios currentTermios;

    // Avoid redundant changes
    if (enable == isNonBlocking) {
        // FIX: Suppress redundant debug messages to reduce clutter
        return;
    }

    // Get current terminal attributes
    if (tcgetattr(STDIN_FILENO, &currentTermios) != 0) {
        pthread_mutex_lock(&coutMutex);
        cerr << "Error: Failed to get terminal attributes: " << strerror(errno) << endl << flush;
        pthread_mutex_unlock(&coutMutex);
        return;
    }

    if (enable) {
        // Save original state if not already saved
        if (!isNonBlocking) {
            originalTermios = currentTermios;
        }
        // Disable canonical mode and echo, set non-blocking
        currentTermios.c_lflag &= ~(ICANON | ECHO);
        currentTermios.c_cc[VMIN] = 0;
        currentTermios.c_cc[VTIME] = 0;
    } else {
        // Restore original terminal settings with explicit canonical mode and echo
        currentTermios = originalTermios;
        currentTermios.c_lflag |= (ICANON | ECHO); // Explicitly enable canonical mode and echo
        currentTermios.c_cc[VMIN] = 1; // Wait for at least one character
        currentTermios.c_cc[VTIME] = 0; // No timeout
    }

    if (tcsetattr(STDIN_FILENO, TCSANOW, &currentTermios) != 0) {
        pthread_mutex_lock(&coutMutex);
        cerr << "Error: Failed to set terminal attributes: " << strerror(errno) << endl << flush;
        pthread_mutex_unlock(&coutMutex);
        return;
    }

    // Update file descriptor flags
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) {
        pthread_mutex_lock(&coutMutex);
        cerr << "Error: Failed to get file descriptor flags: " << strerror(errno) << endl << flush;
        pthread_mutex_unlock(&coutMutex);
        return;
    }
    if (enable) {
        if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
            pthread_mutex_lock(&coutMutex);
            cerr << "Error: Failed to set non-blocking mode: " << strerror(errno) << endl << flush;
            pthread_mutex_unlock(&coutMutex);
        }
    } else {
        if (fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK) == -1) {
            pthread_mutex_lock(&coutMutex);
            cerr << "Error: Failed to set blocking mode: " << strerror(errno) << endl << flush;
            pthread_mutex_unlock(&coutMutex);
        }
    }

    isNonBlocking = enable;
}

int main() {
    srand(time(0));

    // Initialize mutexes and condition variables
    initMutex(coutMutex, "coutMutex");
    initMutex(simulationMutex, "simulationMutex");
    initMutex(arrivalQueueMutex, "arrivalQueueMutex");
    initMutex(departureQueueMutex, "departureQueueMutex");
    initCond(simulationCond, "simulationCond");

    // Save initial terminal state
    if (tcgetattr(STDIN_FILENO, &originalTermios) != 0) {
        cerr << "Error: Failed to save initial terminal attributes: " << strerror(errno) << endl;
        exit(1);
    }

    // Initialize system components
    vector<Airline> airlines;
    int aircraftSequence = 0;

    // Display welcome screen
    displayWelcomeScreen();

    // Login
    if (!login()) {
        cout << "Too many failed attempts. Exiting..." << endl;
        // Cleanup mutexes and condition variables
        destroyMutex(coutMutex, "coutMutex");
        destroyMutex(simulationMutex, "simulationMutex");
        destroyMutex(arrivalQueueMutex, "arrivalQueueMutex");
        destroyMutex(departureQueueMutex, "departureQueueMutex");
        destroyCond(simulationCond, "simulationCond");
        return 1;
    }

    // FIX: Ensure terminal is in blocking mode and input stream is clear post-login
    setNonBlockingInput(false);
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout.flush(); // Ensure all output is displayed

    // Main menu loop
    while (true) {
        // FIX: Clear input stream before displaying menu
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        // Display menu and prompt
        displayMenu();
        pthread_mutex_lock(&coutMutex);
        cout << "Enter your choice: " << flush;
        pthread_mutex_unlock(&coutMutex);

        // Robust input reading
        string input;
        getline(cin, input); // Use getline to avoid issues with residual newlines
        if (input.empty() || input.find_first_not_of(" \t\n") == string::npos) {
            pthread_mutex_lock(&coutMutex);
            cout << "Invalid input. Please enter a number.\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            continue;
        }

        int choice;
        try {
            choice = stoi(input);
        } catch (...) {
            pthread_mutex_lock(&coutMutex);
            cout << "Invalid input. Please enter a number.\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            continue;
        }

        // FIX: Clear input stream after reading choice
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        switch (choice) {
            case 1: // Start Simulation
                if (!simulationRunning) {
                    startSimulation(airlines, runways);
                    pthread_mutex_lock(&coutMutex);
                    cout << "Simulation started (paused until shown).\n" << flush;
                    pthread_mutex_unlock(&coutMutex);
                } else {
                    pthread_mutex_lock(&coutMutex);
                    cout << "Simulation is already running!\n" << flush;
                    pthread_mutex_unlock(&coutMutex);
                }
                break;
            case 2: // End Simulation
                if (simulationRunning) {
                    pthread_mutex_lock(&simulationMutex);
                    simulationRunning = false;
                    simulationPaused = true;
                    globalElapsedTime = 0;
                    // Cancel all aircraft threads
                    for (auto aircraft : activeAircrafts) {
                        pthread_cancel(aircraft->thread);
                        removeAircraftFromEverywhere(aircraft, activeAircrafts, runways);
                    }
                    activeAircrafts.clear();
                    pthread_mutex_lock(&coutMutex);
                    cout << "Simulation terminated.\n" << flush;
                    pthread_mutex_unlock(&coutMutex);
                    pthread_mutex_unlock(&simulationMutex);
                    // Reset airlines and runways
                    airlines.clear();
                    for (auto& runway : runways) {
                        destroyMutex(runway.mutex, "runway mutex");
                        destroyCond(runway.cond, "runway cond");
                    }
                    runways.clear();
                    aircraftStatusMap.clear();
                } else {
                    pthread_mutex_lock(&coutMutex);
                    cout << "No simulation is running!\n" << flush;
                    pthread_mutex_unlock(&coutMutex);
                }
                break;
            case 3: // Spawn Custom Aircraft
                if (simulationRunning) {
                    pthread_mutex_lock(&simulationMutex);
                    simulationPaused = true;
                    pthread_cond_broadcast(&simulationCond);
                    spawnCustomAircraft(airlines, activeAircrafts, aircraftSequence);
                    // FIX: Ensure terminal and input stream are reset
                    setNonBlockingInput(false);
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    pthread_mutex_lock(&coutMutex);
                    cout.flush(); // Ensure output is displayed
                    pthread_mutex_unlock(&coutMutex);
                    pthread_mutex_unlock(&simulationMutex);
                } else {
                    pthread_mutex_lock(&coutMutex);
                    cout << "Please start the simulation first!\n" << flush;
                    pthread_mutex_unlock(&coutMutex);
                }
                break;
            case 4: // Spawn Random Aircraft
                if (simulationRunning) {
                    pthread_mutex_lock(&simulationMutex);
                    simulationPaused = true;
                    pthread_cond_broadcast(&simulationCond);
                    spawnRandomAircraft(airlines, activeAircrafts, aircraftSequence);
                    // FIX: Ensure terminal and input stream are reset
                    setNonBlockingInput(false);
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    pthread_mutex_lock(&coutMutex);
                    cout.flush(); // Ensure output is displayed
                    pthread_mutex_unlock(&coutMutex);
                    pthread_mutex_unlock(&simulationMutex);
                } else {
                    pthread_mutex_lock(&coutMutex);
                    cout << "Please start the simulation first!\n" << flush;
                    pthread_mutex_unlock(&coutMutex);
                }
                break;
            case 5: // Show Simulation
                if (simulationRunning) {
                    showSimulation(airlines, runways, activeAircrafts, aircraftSequence);
                    // FIX: Ensure terminal and input stream are reset after simulation
                    setNonBlockingInput(false);
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout.flush();
                } else {
                    pthread_mutex_lock(&coutMutex);
                    cout << "No simulation is running!\n" << flush;
                    pthread_mutex_unlock(&coutMutex);
                }
                break;
            case 6: // Exit
                if (simulationRunning) {
                    pthread_mutex_lock(&simulationMutex);
                    simulationRunning = false;
                    simulationPaused = true;
                    // Cancel all aircraft threads
                    for (auto aircraft : activeAircrafts) {
                        pthread_cancel(aircraft->thread);
                        removeAircraftFromEverywhere(aircraft, activeAircrafts, runways);
                    }
                    activeAircrafts.clear();
                    pthread_mutex_unlock(&simulationMutex);
                }
                pthread_mutex_lock(&coutMutex);
                cout << "Exiting AirControlX. Goodbye!\n" << flush;
                pthread_mutex_unlock(&coutMutex);
                // Cleanup mutexes and condition variables
                for (auto& runway : runways) {
                    destroyMutex(runway.mutex, "runway mutex");
                    destroyCond(runway.cond, "runway cond");
                }
                destroyMutex(coutMutex, "coutMutex");
                destroyMutex(simulationMutex, "simulationMutex");
                destroyMutex(arrivalQueueMutex, "arrivalQueueMutex");
                destroyMutex(departureQueueMutex, "departureQueueMutex");
                destroyCond(simulationCond, "simulationCond");
                // Restore terminal state
                tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
                return 0;
            default:
                pthread_mutex_lock(&coutMutex);
                cout << "Invalid choice. Please try again.\n" << flush;
                pthread_mutex_unlock(&coutMutex);
        }
    }

    return 0;
}

void displayWelcomeScreen() {
    pthread_mutex_lock(&coutMutex);
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
    cout << "Press Enter to continue..." << flush;
    pthread_mutex_unlock(&coutMutex);
    cin.get();
}

bool login() {
    int attempts = 3;
    string username, password;

    while (attempts > 0) {
        pthread_mutex_lock(&coutMutex);
        cout << "\n=== AirControlX Login ===\n";
        cout << "Username: " << flush;
        pthread_mutex_unlock(&coutMutex);
        getline(cin, username);
        pthread_mutex_lock(&coutMutex);
        cout << "Password: " << flush;
        pthread_mutex_unlock(&coutMutex);
        getline(cin, password);

        if (username == ADMIN_USERNAME && password == ADMIN_PASSWORD) {
            pthread_mutex_lock(&coutMutex);
            cout << "Login successful!\n" << endl << flush;
            pthread_mutex_unlock(&coutMutex);
            // FIX: Clear input stream after successful login
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return true;
        } else {
            attempts--;
            pthread_mutex_lock(&coutMutex);
            cout << "Invalid credentials. " << attempts << " attempts remaining.\n" << endl << flush;
            pthread_mutex_unlock(&coutMutex);
        }
    }
    return false;
}

void displayMenu() {
    pthread_mutex_lock(&coutMutex);
    cout << "\n=== AirControlX Main Menu ===\n";
    cout << "1. Start Simulation\n";
    cout << "2. End Simulation\n";
    cout << "3. Spawn Custom Aircraft\n";
    cout << "4. Spawn Random Aircraft\n";
    cout << "5. Show Simulation\n";
    cout << "6. Exit\n" << flush;
    pthread_mutex_unlock(&coutMutex);
}

void startSimulation(vector<Airline>& airlines, vector<Runway>& runways) {
    pthread_mutex_lock(&simulationMutex);
    if (!simulationRunning) {
        initializeAirlines(airlines); // Ensure airlines are initialized
        initializeRunways(runways);  // Ensure runways are initialized
        simulationRunning = true;
        simulationPaused = true;
    }
    pthread_mutex_unlock(&simulationMutex);
}

void spawnRandomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    pthread_mutex_lock(&coutMutex);
    cout << "\n=== Spawn Random Aircraft ===\n" << flush;
    pthread_mutex_unlock(&coutMutex);

    // Ensure airlines are initialized
    if (airlines.empty()) {
        pthread_mutex_lock(&coutMutex);
        cout << "Error: No airlines initialized. Initializing airlines...\n" << flush;
        pthread_mutex_unlock(&coutMutex);
        initializeAirlines(airlines);
    }

    // Randomly select direction
    Direction direction = static_cast<Direction>(rand() % 4); // NORTH, SOUTH, EAST, WEST

    // Increment sequence
    int currentSequence = ++aircraftSequence;

    // Create aircraft
    Aircraft* newAircraft = createAircraftForAutoEntry(airlines, direction, currentSequence);
    if (!newAircraft) {
        pthread_mutex_lock(&coutMutex);
        cout << "Error: Failed to create aircraft.\n" << flush;
        pthread_mutex_unlock(&coutMutex);
        --aircraftSequence; // Revert sequence increment
        return;
    }

    // Add to status map
    aircraftStatusMap[newAircraft->flightNumber] = newAircraft;

    // Add to active aircrafts
    activeAircrafts.push_back(newAircraft);

    // Create aircraft thread
    int ret = pthread_create(&newAircraft->thread, nullptr, aircraftThreadFunc, newAircraft);
    if (ret != 0) {
        pthread_mutex_lock(&coutMutex);
        cout << "Error: Failed to create thread for " << newAircraft->flightNumber << ": " << strerror(ret) << endl << flush;
        pthread_mutex_unlock(&coutMutex);
        // Clean up
        aircraftStatusMap.erase(newAircraft->flightNumber);
        activeAircrafts.pop_back();
        delete newAircraft;
        --aircraftSequence; // Revert sequence increment
        return;
    }

    // Detach thread for auto-cleanup
    pthread_detach(newAircraft->thread);

    pthread_mutex_lock(&coutMutex);
    cout << "Random flight " << newAircraft->flightNumber << " (" << newAircraft->airline->name
         << ") spawned from " << getDirectionName(direction) << " direction.\n" << flush;
    pthread_mutex_unlock(&coutMutex);
}

Aircraft* createAircraftForAutoEntry(const vector<Airline>& airlines, Direction direction, int seq) {
    if (airlines.empty()) {
        pthread_mutex_lock(&coutMutex);
        cout << "Error: No airlines available in createAircraftForAutoEntry.\n" << flush;
        pthread_mutex_unlock(&coutMutex);
        return nullptr;
    }

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
    vector<const Airline*> eligibleAirlines;
    for (const auto& airline : airlines) {
        if (isEmergency && airline.type == EMERGENCY) {
            eligibleAirlines.push_back(&airline);
        } else if (!isEmergency && airline.type != EMERGENCY) {
            if (!cargoCreated || airline.type != CARGO) {
                eligibleAirlines.push_back(&airline);
            }
        }
    }

    if (eligibleAirlines.empty()) {
        eligibleAirlines.push_back(&airlines[0]); // Fallback to first airline
    }

    const Airline* selectedAirline = eligibleAirlines[rand() % eligibleAirlines.size()];
    if (selectedAirline->type == CARGO) {
        cargoCreated = true;
    }

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
    pthread_mutex_t& queueMutex = (direction == NORTH || direction == SOUTH) ? arrivalQueueMutex : departureQueueMutex;
    priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue = (direction == NORTH || direction == SOUTH) ? arrivalQueue : departureQueue;
    pthread_mutex_lock(&queueMutex);
    if (direction == NORTH || direction == SOUTH) {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        queue.push(aircraft);
    } else {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        queue.push(aircraft);
    }
    pthread_mutex_unlock(&queueMutex);

    return aircraft;
}

void spawnCustomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    pthread_mutex_lock(&coutMutex);
    cout << "\n=== Spawn Custom Aircraft ===\n";
    // Display available airlines
    cout << "Available Airlines:\n";
    for (size_t i = 0; i < airlines.size(); ++i) {
        cout << i + 1 << ". " << airlines[i].name << " (" << getAircraftTypeName(airlines[i].type) << ")\n";
    }
    cout << flush;
    pthread_mutex_unlock(&coutMutex);

    // Get airline choice
    int airlineChoice;
    pthread_mutex_lock(&coutMutex);
    cout << "Select airline (1-" << airlines.size() << "): " << flush;
    pthread_mutex_unlock(&coutMutex);
    string input;
    getline(cin, input);
    try {
        airlineChoice = stoi(input);
    } catch (...) {
        pthread_mutex_lock(&coutMutex);
        cout << "Invalid input.\n" << flush;
        pthread_mutex_unlock(&coutMutex);
        return;
    }
    if (airlineChoice < 1 || airlineChoice > static_cast<int>(airlines.size())) {
        pthread_mutex_lock(&coutMutex);
        cout << "Invalid airline selection.\n" << flush;
        pthread_mutex_unlock(&coutMutex);
        return;
    }
    Airline* selectedAirline = &airlines[airlineChoice - 1];

    // Get flight number
    string flightNumber;
    pthread_mutex_lock(&coutMutex);
    cout << "Enter flight number (e.g., PK101): " << flush;
    pthread_mutex_unlock(&coutMutex);
    getline(cin, flightNumber);

    // Get direction
    pthread_mutex_lock(&coutMutex);
    cout << "Select direction (1. North, 2. South, 3. East, 4. West): " << flush;
    pthread_mutex_unlock(&coutMutex);
    int dirChoice;
    getline(cin, input);
    try {
        dirChoice = stoi(input);
    } catch (...) {
        pthread_mutex_lock(&coutMutex);
        cout << "Invalid input.\n" << flush;
        pthread_mutex_unlock(&coutMutex);
        return;
    }
    Direction direction;
    switch (dirChoice) {
        case 1: direction = NORTH; break;
        case 2: direction = SOUTH; break;
        case 3: direction = EAST; break;
        case 4: direction = WEST; break;
        default:
            pthread_mutex_lock(&coutMutex);
            cout << "Invalid direction.\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            return;
    }

    // Get priority
    pthread_mutex_lock(&coutMutex);
    cout << "Select priority (1. Emergency, 2. High, 3. Normal): " << flush;
    pthread_mutex_unlock(&coutMutex);
    int priChoice;
    getline(cin, input);
    try {
        priChoice = stoi(input);
    } catch (...) {
        pthread_mutex_lock(&coutMutex);
        cout << "Invalid input.\n" << flush;
        pthread_mutex_unlock(&coutMutex);
        return;
    }
    Priority priority;
    switch (priChoice) {
        case 1: priority = EMERGENCY_PRIORITY; break;
        case 2: priority = HIGH_PRIORITY; break;
        case 3: priority = NORMAL_PRIORITY; break;
        default:
            pthread_mutex_lock(&coutMutex);
            cout << "Invalid priority.\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            return;
    }

    // Get scheduled time
    string schedTimeStr;
    pthread_mutex_lock(&coutMutex);
    cout << "Enter scheduled time (YYYY-MM-DD HH:MM): " << flush;
    pthread_mutex_unlock(&coutMutex);
    getline(cin, schedTimeStr);
    struct tm tm = {};
    istringstream ss(schedTimeStr);
    ss >> get_time(&tm, "%Y-%m-%d %H:%M");
    if (ss.fail()) {
        pthread_mutex_lock(&coutMutex);
        cout << "Invalid time format.\n" << flush;
        pthread_mutex_unlock(&coutMutex);
        return;
    }
    time_t scheduledTime = mktime(&tm);

    // Create aircraft
    Aircraft* newAircraft = createAircraftForManualEntry(airlines, direction, ++aircraftSequence, flightNumber, selectedAirline->type, priority, scheduledTime);
    activeAircrafts.push_back(newAircraft);

    // Add to status map
    aircraftStatusMap[newAircraft->flightNumber] = newAircraft;

    // Create aircraft thread
    int ret = pthread_create(&newAircraft->thread, nullptr, aircraftThreadFunc, newAircraft);
    if (ret != 0) {
        pthread_mutex_lock(&coutMutex);
        cout << "Failed to create thread for " << flightNumber << ": " << strerror(ret) << endl << flush;
        pthread_mutex_unlock(&coutMutex);
        aircraftStatusMap.erase(newAircraft->flightNumber);
        activeAircrafts.pop_back();
        delete newAircraft;
        return;
    }
    pthread_detach(newAircraft->thread); // Detach thread for auto-cleanup

    pthread_mutex_lock(&coutMutex);
    cout << "Flight " << flightNumber << " spawned successfully.\n" << flush;
    pthread_mutex_unlock(&coutMutex);
}

void showSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) {
    setNonBlockingInput(true);
    pthread_mutex_lock(&coutMutex);
    cout << "Showing simulation. Press 'q' to return to menu.\n" << flush;
    pthread_mutex_unlock(&coutMutex);

    // Resume simulation
    pthread_mutex_lock(&simulationMutex);
    simulationPaused = false;
    pthread_cond_broadcast(&simulationCond);
    pthread_mutex_unlock(&simulationMutex);

    auto startTime = chrono::steady_clock::now();
    while (simulationRunning && globalElapsedTime < SIMULATION_DURATION) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5; // 5-second timeout
        int ret = pthread_mutex_timedlock(&simulationMutex, &ts);
        if (ret == ETIMEDOUT) {
            pthread_mutex_lock(&coutMutex);
            cout << "Error: simulationMutex lock timed out in showSimulation\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            break;
        } else if (ret != 0) {
            pthread_mutex_lock(&coutMutex);
            cout << "Error: Failed to lock simulationMutex: " << strerror(ret) << "\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            break;
        }

        // Check for 'q' key
        if (kbhit()) {
            pthread_mutex_lock(&coutMutex);
            cout << "Returning to menu.\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            simulationPaused = true;
            pthread_cond_broadcast(&simulationCond);
            pthread_mutex_unlock(&simulationMutex);
            break;
        }

        updateSimulationStep(airlines, runways, activeAircrafts);
        displayStatus(activeAircrafts, runways, globalElapsedTime);
        globalElapsedTime += TIME_STEP;

        pthread_mutex_unlock(&simulationMutex);
        usleep(TIME_STEP * 1000000); // Sleep for TIME_STEP seconds

        // Check timeout
        auto elapsed = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime).count();
        if (elapsed >= 30) {
            pthread_mutex_lock(&coutMutex);
            cout << "Timeout: Returning to menu.\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            pthread_mutex_lock(&simulationMutex);
            simulationPaused = true;
            pthread_cond_broadcast(&simulationCond);
            pthread_mutex_unlock(&simulationMutex);
            break;
        }
    }

    setNonBlockingInput(false); // Restore blocking input
}

void* aircraftThreadFunc(void* arg) {
    Aircraft* aircraft = static_cast<Aircraft*>(arg);

    while (simulationRunning && !aircraft->isFaulty &&
           aircraft->phase != AT_GATE_BRS && aircraft->phase != DEPARTURE_CRUISE_BRS) {
        pthread_mutex_lock(&simulationMutex);
        // Pause if simulation is not being shown
        while (simulationPaused && simulationRunning) {
            pthread_cond_wait(&simulationCond, &simulationMutex);
        }
        if (!simulationRunning) {
            pthread_mutex_unlock(&simulationMutex);
            break;
        }

        // Update aircraft status and simulate
        updateAircraftStatus(*aircraft);
        updateAircraftSpeed(*aircraft);
        checkForViolations(*aircraft);
        if (aircraft->direction == NORTH || aircraft->direction == SOUTH) {
            simulateArrival(*aircraft, runways);
        } else {
            simulateDeparture(*aircraft, runways);
        }
        aircraft->timeInFlight += TIME_STEP;

        // Check fuel status for priority update
        if (aircraft->fuelStatus < 30 && aircraft->priority == NORMAL_PRIORITY) {
            aircraft->priority = HIGH_PRIORITY;
            pthread_mutex_lock(&coutMutex);
            cout << "Flight " << aircraft->flightNumber << " upgraded to HIGH_PRIORITY due to low fuel.\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            // Rebuild queue
            pthread_mutex_t& queueMutex = (aircraft->direction == NORTH || aircraft->direction == SOUTH) ? arrivalQueueMutex : departureQueueMutex;
            priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue = (aircraft->direction == NORTH || aircraft->direction == SOUTH) ? arrivalQueue : departureQueue;
            rebuildQueue(aircraft, queue, queueMutex);
        }

        pthread_mutex_unlock(&simulationMutex);
        usleep(TIME_STEP * 1000000); // Sleep for TIME_STEP seconds
    }

    // Aircraft completed or faulty, clean up
    pthread_mutex_lock(&simulationMutex);
    if (aircraft->phase == AT_GATE_BRS || aircraft->phase == DEPARTURE_CRUISE_BRS || aircraft->isFaulty) {
        removeAircraftFromEverywhere(aircraft, activeAircrafts, runways);
    }
    pthread_mutex_unlock(&simulationMutex);

    return nullptr;
}

void rebuildQueue(Aircraft* aircraft, priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue, pthread_mutex_t& queueMutex) {
    pthread_mutex_lock(&queueMutex);
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
    pthread_mutex_unlock(&queueMutex);
}

void updateSimulationStep(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts) {
    updateWaitTimes();

    // Handle random ground faults every 30 seconds
    if (globalElapsedTime % 30 == 0) {
        handleGroundFaults(activeAircrafts);
    }

    // Remove completed aircraft every 7 seconds
    if (globalElapsedTime % 7 == 0 && globalElapsedTime != 0) {
        for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) {
            if ((*it)->phase == AT_GATE_BRS || (*it)->phase == DEPARTURE_CRUISE_BRS) {
                removeAircraftFromEverywhere(*it, activeAircrafts, runways);
                it = activeAircrafts.begin(); // Reset iterator after erase
            } else {
                ++it;
            }
        }
    }
}

bool kbhit() {
    char c;
    ssize_t nread = read(STDIN_FILENO, &c, 1);
    if (nread == 1) {
        return c == 'q';
    } else if (nread == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        pthread_mutex_lock(&coutMutex);
        cout << "Error in kbhit: " << strerror(errno) << "\n" << flush;
        pthread_mutex_unlock(&coutMutex);
    }
    return false;
}

void initializeAirlines(vector<Airline>& airlines) {
    airlines.clear();
    airlines.push_back({"PIA", COMMERCIAL, 6, 4, 0});
    airlines.push_back({"AirBlue", COMMERCIAL, 4, 4, 0});
    airlines.push_back({"FedEx", CARGO, 3, 2, 0});
    airlines.push_back({"Pakistan Airforce", EMERGENCY, 2, 1, 0});
    airlines.push_back({"Blue Dart", CARGO, 2, 2, 0});
    airlines.push_back({"AghaKhan Air Ambulance", EMERGENCY, 2, 1, 0});
}

void initializeRunways(vector<Runway>& runways) {
    runways.clear();
    runways.push_back({RWY_A, "RWY-A (North-South Arrivals)", false, nullptr, queue<Aircraft*>(), PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER});
    runways.push_back({RWY_B, "RWY-B (East-West Departures)", false, nullptr, queue<Aircraft*>(), PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER});
    runways.push_back({RWY_C, "RWY-C (Cargo/Emergency/Overflow)", false, nullptr, queue<Aircraft*>(), PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER});
    for (auto& runway : runways) {
        initMutex(runway.mutex, "runway mutex");
        initCond(runway.cond, "runway cond");
    }
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
    pthread_mutex_t& queueMutex = (direction == NORTH || direction == SOUTH) ? arrivalQueueMutex : departureQueueMutex;
    priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue = (direction == NORTH || direction == SOUTH) ? arrivalQueue : departureQueue;
    pthread_mutex_lock(&queueMutex);
    if (direction == NORTH || direction == SOUTH) {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        queue.push(aircraft);
    } else {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        queue.push(aircraft);
    }
    pthread_mutex_unlock(&queueMutex);

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

                // Free the runway and signal next aircraft
                for (auto& runway : runways) {
                    if (runway.currentAircraft == &aircraft) {
                        pthread_mutex_lock(&runway.mutex);
                        runway.isOccupied = false;
                        runway.currentAircraft = nullptr;
                        pthread_cond_broadcast(&runway.cond);
                        pthread_mutex_unlock(&runway.mutex);
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

                // Free the runway and signal next aircraft
                for (auto& runway : runways) {
                    if (runway.currentAircraft == &aircraft) {
                        pthread_mutex_lock(&runway.mutex);
                        runway.isOccupied = false;
                        runway.currentAircraft = nullptr;
                        pthread_cond_broadcast(&runway.cond);
                        pthread_mutex_unlock(&runway.mutex);
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
    // Check if already assigned
    for (auto& runway : runways) {
        pthread_mutex_lock(&runway.mutex);
        if (runway.currentAircraft == &aircraft) {
            pthread_mutex_unlock(&runway.mutex);
            return &runway;
        }
        queue<Aircraft*> tempQueue = runway.waitingQueue;
        while (!tempQueue.empty()) {
            if (tempQueue.front() == &aircraft) {
                pthread_mutex_unlock(&runway.mutex);
                return &runway;
            }
            tempQueue.pop();
        }
        pthread_mutex_unlock(&runway.mutex);
    }

    // Assign runway based on priority and direction
    Runway* assignedRunway = nullptr;
    if (aircraft.priority == EMERGENCY_PRIORITY) {
        // Try RWY-C, then RWY-A/B, then shortest queue
        for (int i : {RWY_C, RWY_A, RWY_B}) {
            Runway& runway = runways[i];
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2; // 2-second timeout
            int ret = pthread_mutex_timedlock(&runway.mutex, &ts);
            if (ret == ETIMEDOUT || ret != 0) {
                continue;
            }
            if (!runway.isOccupied) {
                runway.isOccupied = true;
                runway.currentAircraft = &aircraft;
                assignedRunway = &runway;
                pthread_mutex_unlock(&runway.mutex);
                break;
            }
            pthread_mutex_unlock(&runway.mutex);
        }
        if (!assignedRunway) {
            // Join shortest queue
            Runway* shortest = &runways[RWY_C];
            size_t minQueueSize = shortest->waitingQueue.size();
            for (auto& runway : runways) {
                pthread_mutex_lock(&runway.mutex);
                if (runway.waitingQueue.size() < minQueueSize) {
                    minQueueSize = runway.waitingQueue.size();
                    shortest = &runway;
                }
                pthread_mutex_unlock(&runway.mutex);
            }
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            int ret = pthread_mutex_timedlock(&shortest->mutex, &ts);
            if (ret == ETIMEDOUT || ret != 0) {
                return nullptr;
            }
            shortest->waitingQueue.push(&aircraft);
            aircraft.status = "Waiting for Runway";
            pthread_cond_wait(&shortest->cond, &shortest->mutex);
            pthread_mutex_unlock(&shortest->mutex);
            return nullptr;
        }
    } else if (aircraft.type == CARGO) {
        Runway& runway = runways[RWY_C];
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        int ret = pthread_mutex_timedlock(&runway.mutex, &ts);
        if (ret == ETIMEDOUT || ret != 0) {
            return nullptr;
        }
        if (!runway.isOccupied) {
            runway.isOccupied = true;
            runway.currentAircraft = &aircraft;
            assignedRunway = &runway;
            pthread_mutex_unlock(&runway.mutex);
        } else {
            runway.waitingQueue.push(&aircraft);
            aircraft.status = "Waiting for Runway";
            pthread_cond_wait(&runway.cond, &runway.mutex);
            pthread_mutex_unlock(&runway.mutex);
            return nullptr;
        }
    } else if (aircraft.direction == NORTH || aircraft.direction == SOUTH) {
        // Try RWY-A, then RWY-C
        for (int i : {RWY_A, RWY_C}) {
            Runway& runway = runways[i];
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            int ret = pthread_mutex_timedlock(&runway.mutex, &ts);
            if (ret == ETIMEDOUT || ret != 0) {
                continue;
            }
            if (!runway.isOccupied) {
                runway.isOccupied = true;
                runway.currentAircraft = &aircraft;
                assignedRunway = &runway;
                pthread_mutex_unlock(&runway.mutex);
                break;
            }
            pthread_mutex_unlock(&runway.mutex);
        }
        if (!assignedRunway) {
            Runway* shortest = &runways[RWY_A];
            pthread_mutex_lock(&runways[RWY_C].mutex);
            if (runways[RWY_C].waitingQueue.size() < shortest->waitingQueue.size()) {
                shortest = &runways[RWY_C];
            }
            pthread_mutex_unlock(&runways[RWY_C].mutex);
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            int ret = pthread_mutex_timedlock(&shortest->mutex, &ts);
            if (ret == ETIMEDOUT || ret != 0) {
                return nullptr;
            }
            shortest->waitingQueue.push(&aircraft);
            aircraft.status = "Waiting for Runway";
            pthread_cond_wait(&shortest->cond, &shortest->mutex);
            pthread_mutex_unlock(&shortest->mutex);
            return nullptr;
        }
    } else {
        // Try RWY-B, then RWY-C
        for (int i : {RWY_B, RWY_C}) {
            Runway& runway = runways[i];
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            int ret = pthread_mutex_timedlock(&runway.mutex, &ts);
            if (ret == ETIMEDOUT || ret != 0) {
                continue;
            }
            if (!runway.isOccupied) {
                runway.isOccupied = true;
                runway.currentAircraft = &aircraft;
                assignedRunway = &runway;
                pthread_mutex_unlock(&runway.mutex);
                break;
            }
            pthread_mutex_unlock(&runway.mutex);
        }
        if (!assignedRunway) {
            Runway* shortest = &runways[RWY_B];
            pthread_mutex_lock(&runways[RWY_C].mutex);
            if (runways[RWY_C].waitingQueue.size() < shortest->waitingQueue.size()) {
                shortest = &runways[RWY_C];
            }
            pthread_mutex_unlock(&runways[RWY_C].mutex);
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            int ret = pthread_mutex_timedlock(&shortest->mutex, &ts);
            if (ret == ETIMEDOUT || ret != 0) {
                return nullptr;
            }
            shortest->waitingQueue.push(&aircraft);
            aircraft.status = "Waiting for Runway";
            pthread_cond_wait(&shortest->cond, &shortest->mutex);
            pthread_mutex_unlock(&shortest->mutex);
            return nullptr;
        }
    }
    return assignedRunway;
}

void updateWaitTimes() {
    pthread_mutex_lock(&simulationMutex);
    for (auto& pair : aircraftStatusMap) {
        Aircraft* aircraft = pair.second;
        if (aircraft->status == "Waiting for Runway") {
            aircraft->estimatedWaitTime++;
            // Aging: Upgrade NORMAL_PRIORITY to HIGH_PRIORITY after AGING_THRESHOLD
            if (aircraft->estimatedWaitTime >= AGING_THRESHOLD && aircraft->priority == NORMAL_PRIORITY) {
                aircraft->priority = HIGH_PRIORITY;
                pthread_mutex_lock(&coutMutex);
                cout << "Flight " << aircraft->flightNumber << " upgraded to HIGH_PRIORITY due to aging.\n" << flush;
                pthread_mutex_unlock(&coutMutex);
                // Rebuild queue
                pthread_mutex_t& queueMutex = (aircraft->direction == NORTH || aircraft->direction == SOUTH) ? arrivalQueueMutex : departureQueueMutex;
                priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue = (aircraft->direction == NORTH || aircraft->direction == SOUTH) ? arrivalQueue : departureQueue;
                rebuildQueue(aircraft, queue, queueMutex);
            }
        }
    }
    pthread_mutex_unlock(&simulationMutex);
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
        pthread_mutex_lock(&coutMutex);
        cout << "VIOLATION: " << aircraft.flightNumber << " (" << aircraft.airline->name
             << ") exceeded speed limits in " << getPhaseName(aircraft.phase)
             << " phase (Speed: " << aircraft.speed << " km/h)" << endl << flush;
        pthread_mutex_unlock(&coutMutex);
    }
}

void handleGroundFaults(vector<Aircraft*>& aircrafts) {
    for (auto aircraft : aircrafts) {
        // 5% chance of ground fault for each aircraft every 30 seconds
        if ((rand() % 100) < 5 && !aircraft->isFaulty) {
            aircraft->isFaulty = true;
            aircraft->status = "Ground Fault";
            pthread_mutex_lock(&coutMutex);
            cout << "GROUND FAULT: " << aircraft->flightNumber << " (" << aircraft->airline->name
                 << ") has encountered a ground fault and is out of service.\n" << flush;
            pthread_mutex_unlock(&coutMutex);
            // Remove from runways and queues
            removeAircraftFromEverywhere(aircraft, aircrafts, runways);
        }
    }
}

void displayStatus(const vector<Aircraft*>& aircrafts, const vector<Runway>& runways, int elapsedTime) {
    pthread_mutex_lock(&coutMutex);
    cout << "\n=== Simulation Status (Time: " << elapsedTime << "s) ===\n";

    // Display runway status
    cout << "\nRunway Status:\n";
    for (const auto& runway : runways) {
        cout << runway.name << ": ";
        if (runway.isOccupied && runway.currentAircraft) {
            cout << "Occupied by " << runway.currentAircraft->flightNumber << " (" << runway.currentAircraft->status << ")";
        } else {
            cout << "Available";
        }
        cout << ", Waiting: " << runway.waitingQueue.size() << " aircraft\n";
    }

    // Display aircraft status
    cout << "\nAircraft Status:\n";
    for (const auto& aircraft : aircrafts) {
        cout << "Flight " << aircraft->flightNumber << " (" << aircraft->airline->name << "): "
             << aircraft->status << ", Phase: " << getPhaseName(aircraft->phase)
             << ", Speed: " << aircraft->speed << " km/h"
             << ", Fuel: " << aircraft->fuelStatus << "%"
             << ", Priority: ";
        switch (aircraft->priority) {
            case EMERGENCY_PRIORITY: cout << "Emergency"; break;
            case HIGH_PRIORITY: cout << "High"; break;
            case NORMAL_PRIORITY: cout << "Normal"; break;
        }
        cout << ", Wait Time: " << aircraft->estimatedWaitTime << "s\n";
    }

    // Display queue sizes
    cout << "\nQueue Status:\n";
    cout << "Arrival Queue: " << arrivalQueue.size() << " aircraft\n";
    cout << "Departure Queue: " << departureQueue.size() << " aircraft\n";

    cout << "====================================\n" << flush;
    pthread_mutex_unlock(&coutMutex);
}

void removeAircraftFromEverywhere(Aircraft* aircraft, vector<Aircraft*>& aircrafts, vector<Runway>& runways) {
    // Remove from aircraftStatusMap
    pthread_mutex_lock(&simulationMutex);
    aircraftStatusMap.erase(aircraft->flightNumber);

    // Remove from activeAircrafts
    auto it = find(aircrafts.begin(), aircrafts.end(), aircraft);
    if (it != aircrafts.end()) {
        aircrafts.erase(it);
    }

    // Remove from arrival or departure queue
    priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> tempQueue;
    pthread_mutex_lock(&arrivalQueueMutex);
    while (!arrivalQueue.empty()) {
        Aircraft* a = arrivalQueue.top();
        arrivalQueue.pop();
        if (a != aircraft) {
            tempQueue.push(a);
        }
    }
    arrivalQueue = tempQueue;
    pthread_mutex_unlock(&arrivalQueueMutex);

    tempQueue = priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>();
    pthread_mutex_lock(&departureQueueMutex);
    while (!departureQueue.empty()) {
        Aircraft* a = departureQueue.top();
        departureQueue.pop();
        if (a != aircraft) {
            tempQueue.push(a);
        }
    }
    departureQueue = tempQueue;
    pthread_mutex_unlock(&departureQueueMutex);

    // Remove from runway assignments and queues
    for (auto& runway : runways) {
        pthread_mutex_lock(&runway.mutex);
        if (runway.currentAircraft == aircraft) {
            runway.isOccupied = false;
            runway.currentAircraft = nullptr;
            pthread_cond_broadcast(&runway.cond);
        }
        queue<Aircraft*> newQueue;
        while (!runway.waitingQueue.empty()) {
            Aircraft* a = runway.waitingQueue.front();
            runway.waitingQueue.pop();
            if (a != aircraft) {
                newQueue.push(a);
            }
        }
        runway.waitingQueue = newQueue;
        pthread_mutex_unlock(&runway.mutex);
    }

    // Output removal
    pthread_mutex_lock(&coutMutex);
    cout << "Aircraft " << aircraft->flightNumber << " removed from simulation.\n" << flush;
    pthread_mutex_unlock(&coutMutex);

    // Free aircraft memory
    delete aircraft;
    pthread_mutex_unlock(&simulationMutex);
}