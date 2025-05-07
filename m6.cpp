/*

---------   MUHAMMAD FATIK BIN IMRAN (23i-0655)
---------   MUHAMMAD KALEEM AKHTAR (23i-0524)
-----   SECTION: BCS-4C

*/

// I/O Libraries:
#include <iostream>
#include <iomanip>

// Data Structures:
#include <vector>
#include <string>
#include <map>

// C Libraries:
#include <ctime>
#include <cstdlib>
#include <chrono>

// OS Libraries:
#include <thread>
#include <sstream>
#include <mutex>
#include <queue>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex>
#include <limits>

// OTHERS:
#include <cmath>

// GRAPHICS:
#include <SFML/Graphics.hpp>

using namespace std;

// Gloabl Constants
const int SIMULATION_DURATION = 300;       
const int TIME_STEP = 1;                   
const string ADMIN_USERNAME = "admin";
const string ADMIN_PASSWORD = "atcs2025";
const int AGING_THRESHOLD = 30;            
const int AVG_RUNWAY_USAGE_TIME = 5;       
const double COMMERCIAL_FINE = 500000.0;   
const double CARGO_FINE = 700000.0;        
const double SERVICE_FEE = 0.15;           
const int DUE_DAYS = 3;                    

enum AircraftType { COMMERCIAL, CARGO, EMERGENCY };
enum FlightPhase {
    HOLDING, APPROACH, LANDING, TAXI, AT_GATE,     
    TAKEOFF_ROLL, CLIMB, DEPARTURE_CRUISE,         
    AT_GATE_BRS, DEPARTURE_CRUISE_BRS,
    ACCELERATING_TO_CRUISE,
};
enum Direction { NORTH, SOUTH, EAST, WEST };
enum RunwayID { RWY_A, RWY_B, RWY_C };
enum Priority { EMERGENCY_PRIORITY, HIGH_PRIORITY, NORMAL_PRIORITY };

/* 
    OOP Concepts (Efficient coding :) )
    STRUCTS: 
*/

struct AVN 
{
    string avnID;              // here is the format we decided:  AVN-20250507-001
    string flightNumber;
    string airlineName;
    AircraftType aircraftType;
    double speedRecorded;       
    string speedPermissible;    
    double altitudeRecorded;    
    string altitudePermissible; 
    time_t issueTime;
    int fineAmount;
    string paymentStatus;      // we have generally used: "unpaid", "overdue", or "paid"
    time_t dueDate;
};


struct Airline 
{
    string name;
    AircraftType type;
    int aircraftCount;
    int flightCount;
    int violations;
};




struct Aircraft
{
    string flightNumber;
     Airline* airline;
    AircraftType type;
    Direction direction;
    bool isEmergency;
    bool hasAVN;
    
    FlightPhase phase;
    double speed; 
    bool isFaulty;
    int timeInFlight = 0;
    int fuelStatus; 
    
    Priority priority;
    time_t scheduledTime;
    int actualWaitTime = 0;
    int estimatedWaitTime = 0;
    
    string status = "Waiting";
    double altitude = 0;
};



struct Runway 
{
    RunwayID id;
    string name;
    bool isOccupied;
    Aircraft* currentAircraft;
    queue<Aircraft*> waitingQueue;  // Ensuring that each runway has its own waiting queue
};



struct ComparePriority 
{
    bool operator()(Aircraft* aircraft1, Aircraft* aircraft2) 
    {
        if (aircraft1->priority == aircraft2->priority) 
            return aircraft1->scheduledTime > aircraft2->scheduledTime;     // FCFS within same priority
        return aircraft1->priority > aircraft2->priority;
    }
};


/*
    +++ Global variables +++
*/

// MUTEXES:
mutex coutMutex;
mutex simulationMutex;
mutex avnListMutex;


priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> arrivalQueue;
priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> departureQueue;
map<string, Aircraft*> aircraftStatusMap;

// BOOLS:
bool cargoCreated = false;
bool simulationRunning = false;
bool simulationPaused = true; 

thread simulationThread;
int globalElapsedTime = 0; vector<AVN> avnList;       
static int avnSequence = 0; 



void clearInputBuffer() { cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');}

bool isValidInteger(const string& str, int& value, int min, int max) 
{
    if (str.empty()) return false;

    int i = 0;
    bool isNegative = (str[0] == '-');
    if (isNegative) i = 1;

    for (; i < str.length(); i++)
        if (!isdigit(str[i]))
            return false;
        
    value = atoi(str.c_str());
    if (isNegative) value = -value;

    if (value >= min && value <= max)
        return true;
    return false;
}

// Validating time format (YYYY-MM-DD HH:MM) and ranges
bool isValidTimeFormat(const string& timeStr, struct tm& tm) 
{
    regex timeRegex("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}");
    if (!regex_match(timeStr, timeRegex))
        return false;
    
    istringstream ss(timeStr);
    ss >> get_time(&tm, "%Y-%m-%d %H:%M");
    if (ss.fail())
        return false;
    
    
    if (tm.tm_year < 125)  // Year >= 2025 (tm_year is years since 1900)
        return false;
    
    if (tm.tm_mon < 0 || tm.tm_mon > 11) // Months 0-11
        return false;
    
    if (tm.tm_hour < 0 || tm.tm_hour > 23) 
        return false;
    
    if (tm.tm_min < 0 || tm.tm_min > 59) 
        return false;
        
    int daysInMonth[] = {31, (tm.tm_year % 4 == 0 && tm.tm_year % 100 != 0) || (tm.tm_year % 400 == 0) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (tm.tm_mday < 1 || tm.tm_mday > daysInMonth[tm.tm_mon]) 
        return false;    

    return true;
}



/* -----------
    PROTOTYPES
 ------------ */
void displayWelcomeScreen();
bool login();
void displayMenu();

void spawnCustomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void spawnRandomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);

void startSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void runSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);
void showSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence);

void accessAirlinePortal();
bool airlineLogin(string& airlineName);
void displayAirlineMenu();
void viewAllAVNs(const string& airlineName);
void viewSpecificAVN(const string& airlineName);
void initiatePayment(const string& airlineName);
void mockStripePay(const AVN& avn);

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
void generateAVN(Aircraft& aircraft, bool speedViolation, bool altitudeViolation);
void updateAVNPaymentStatus(const string& avnID);

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


// A simple f/n to  Set terminal to non-blocking input mode
void setNonBlockingInput(bool enable) 
{
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    if (enable) 
    {
        ttystate.c_lflag &= ~(ICANON | ECHO);
        ttystate.c_cc[VMIN] = 0;
        ttystate.c_cc[VTIME] = 0;
    } 
    else 
        ttystate.c_lflag |= (ICANON | ECHO);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (enable) 
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    else 
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    
}

void cleanupSimulation(vector<Aircraft*>& activeAircrafts, vector<Runway>& runways) 
{
    lock_guard<mutex> guard(simulationMutex);
    simulationRunning = false;
    simulationPaused = true;
    globalElapsedTime = 0;

    if (simulationThread.joinable()) 
        simulationThread.join();

    vector<Aircraft*> aircraftToDelete = activeAircrafts;

    // Clearing runways:
    for (auto& runway : runways) 
    {
        runway.isOccupied = false;
        runway.currentAircraft = nullptr;
        while (!runway.waitingQueue.empty()) 
            runway.waitingQueue.pop();        
    }

    // Clearing priority queues:
    while (!arrivalQueue.empty()) 
        arrivalQueue.pop();    

    while (!departureQueue.empty())
        departureQueue.pop();
    
    // Clearing active aircrafts & status map
    activeAircrafts.clear();
    aircraftStatusMap.clear();

    // Ensuring all aircrafts deleted
    for (auto aircraft : aircraftToDelete) 
        delete aircraft;

    avnSequence = 0;
    avnList.clear();
    cargoCreated = false;
}


int main() 
{
    srand(time(0));

    vector<Airline> airlines;
    vector<Runway> runways;
    vector<Aircraft*> activeAircrafts;
    int aircraftSequence = 0;

    displayWelcomeScreen();

    if (!login()) 
    {
        cout << "Too many failed attempts. Exiting..." << endl;
        return 1;
    }

    // THE MAIN LOOP:
    while (true) 
    {
        setNonBlockingInput(false);
        displayMenu();
        int choice;
        string input;
    
        while (true) 
        {
            cout << "Enter your choice: ";
            getline(cin, input);
            if (isValidInteger(input, choice, 1, 7))
                break;            
            cout << "Invalid input. Please enter a number between 1 and 7. Press Enter to continue...\n";
            clearInputBuffer();
        }

        switch (choice) 
        {
            case 1: // Starting Simulation
                if (!simulationRunning) 
                {
                    initializeAirlines(airlines);
                    initializeRunways(runways);
                    simulationRunning = true;
                    simulationPaused = true; // Start paused
                    simulationThread = thread(runSimulation, ref(airlines), ref(runways), ref(activeAircrafts), ref(aircraftSequence));
                    simulationThread.detach();
                    cout << "Simulation started (paused until shown).\n";
                }
                else 
                    cout << "Simulation is already running!\n";                
                break;


            case 2: // Ending Simulation
                if (simulationRunning) 
                {
                    displayAVNStatistics(airlines);
                    cleanupSimulation(activeAircrafts, runways);
                    airlines.clear();
                    runways.clear();
                    cout << "Simulation terminated.\n";
                } 
                else 
                    cout << "No simulation is running!\n";
                break;


            case 3: // Spawning aCustom Aircraft
                if (simulationRunning) 
                { 
                    lock_guard<mutex> guard(simulationMutex);
                    simulationPaused = true;
                    spawnCustomAircraft(airlines, activeAircrafts, aircraftSequence);
                }
                else 
                    cout << "Please start the simulation first!\n";
                break;


            case 4: // Spawn Random Aircraft
                if (simulationRunning) 
                {
                    lock_guard<mutex> guard(simulationMutex);
                    simulationPaused = true;
                    spawnRandomAircraft(airlines, activeAircrafts, aircraftSequence);
                } 
                else                 
                    cout << "Dear, start the simulation first!\n";  
                break;

            case 5: // Show Simulation
                if (simulationRunning) 
                    showSimulation(airlines, runways, activeAircrafts, aircraftSequence);
                else 
                    cout << "No simulation is running!\n";
                break;


            case 6: // Bye Bye
                if (simulationRunning) 
                {
                    displayAVNStatistics(airlines);
                    cleanupSimulation(activeAircrafts, runways);
                }
                cout << "Exiting AirControlX. Goodbye!\n";
                return 0;


			case 7:
        	{
	            lock_guard<mutex> guard(simulationMutex);
	            simulationPaused = true;
	            accessAirlinePortal();
       		}
        	break;
            default:
                cout << "Invalid choice. Please try again. It's not that hard!\n";
        }
    }

    return 0;
}



void displayWelcomeScreen() 
{
    cout << "\033[1;36m"; // Cyan color
    cout << R"(
    ╔══════════════════════════════════════════════════════╗
    ║                                                      ║
    ║       █████╗ ██╗  ██╗ ███████╗     █████╗ ████████╗ ██████╗ ███████╗    
    ║      ██╔══██╗██║ ██╔╝ ██╔════╝    ██╔══██╗╚══██╔══╝██╔════╝ ██╔════╝     
    ║      ███████║█████╔╝  █████╗      ███████║   ██║   ██║      ███████╗    
    ║      ██╔══██║██╔═██╗  ██╔══╝      ██╔══██║   ██║   ██║       ╚════██║     
    ║      ██║  ██║██║  ██╗ ██║         ██║  ██║   ██║   ╚██████╔  ███████║    
    ║      ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═╝         ╚═╝  ╚═╝   ╚═╝    ╚═════╝  ╚══════╝    
    ║                                                      ║
    ║           Automated Air Traffic Control System       ║
    ║           Developed by: Muhammad Fatik & Kaleem      ║
    ║                                                      ║
    ╚══════════════════════════════════════════════════════╝
    )" << endl;
    cout << "\033[0m"; // Reset color
    cout << "Press Enter to continue...";
    cin.get();
}


bool login() 
{
    int attempts = 3;
    string username, password;

    while (attempts > 0) 
    {
        lock_guard<mutex> guard(coutMutex);
        cout << "\n=== AirControlX Login ===\n";
        // Validate username
        while (true)
        {
            cout << "Username: ";
            getline(cin, username);
            if (!username.empty() && username.length() <= 50) 
                break;
            cout << "Invalid username. Must be non-empty and less than 50 characters. Press Enter to continue...\n";
            clearInputBuffer();
        }

        // Validate password
        while (true)
        {
            cout << "Password: ";
            getline(cin, password);
            if (!password.empty() && password.length() <= 50) 
                break;
            cout << "Invalid password. Must be non-empty and less than 50 characters. Press Enter to continue...\n";
            clearInputBuffer();
        }

        if (username == ADMIN_USERNAME && password == ADMIN_PASSWORD) 
        {
            cout << "Login successful!\n" << endl;
            return true;
        } 
        else 
        {
            attempts--;
            cout << "Invalid credentials. " << attempts << " attempts remaining.\n" << endl;
        }
    }
    return false;
}

void displayMenu() 
{
    lock_guard<mutex> guard(coutMutex);
    cout << "\n=== AirControlX Main Menu ===\n";
    cout << "1. Start Simulation\n";
    cout << "2. End Simulation\n";
    cout << "3. Spawn Custom Aircraft\n";
    cout << "4. Spawn Random Aircraft\n";
    cout << "5. Show Simulation\n";
    cout << "6. Exit\n";
    cout << "7. Access Airline Portal\n";
}

void accessAirlinePortal() 
{
    string airlineName;
    if (airlineLogin(airlineName)) 
    {
        while (true) 
        {
            displayAirlineMenu();
            int choice;
            string input;
            //lock_guard<mutex> guard(coutMutex);
            while (true) 
            {
                cout << "Enter your choice: ";
                getline(cin, input);
                if (isValidInteger(input, choice, 1, 4)) {
                    break;
                }
                cout << "Invalid input. Please enter a number between 1 and 4. Press Enter to continue...\n";
                clearInputBuffer();
            }

            switch (choice) 
            {
                case 1: // View All AVNs
                    viewAllAVNs(airlineName);
                    break;
                case 2: // View Specific AVN
                    viewSpecificAVN(airlineName);
                    break;
                case 3: // Initiate Payment
                    initiatePayment(airlineName);
                    break;
                case 4: // Logout
                    {lock_guard<mutex> guard(coutMutex);
                    cout << "Logged out from Airline Portal.\n";}
                    return;
                default:
                    {lock_guard<mutex> guard(coutMutex);
                    cout << "Invalid choice. Please try again.\n";}
            }
        }
    }
}

bool airlineLogin(string& airlineName) 
{
    lock_guard<mutex> guard(coutMutex);
    cout << "\n=== Airline Portal Login ===\n";

    int attempts = 3;
    string flightNumber, avnID;

    while (attempts > 0) 
    {
        while (true) 
        {
            cout << "Enter Flight Number (e.g., PK101): ";
            getline(cin, flightNumber);
            if (!flightNumber.empty() && flightNumber.length() <= 10) 
                break;
            cout << "Invalid flight number. Must be non-empty and less than 10 characters. Press Enter to continue...\n";
            clearInputBuffer();
        }

        // Get AVN ID
        while (true) 
        {
            cout << "Enter AVN ID (e.g., AVN-20250507-001): ";
            getline(cin, avnID);
            regex avnRegex("AVN-\\d{8}-\\d{3}");
            if (regex_match(avnID, avnRegex)) 
                break;
            cout << "Invalid AVN ID format. Use AVN-YYYYMMDD-NNN. Press Enter to continue...\n";
            clearInputBuffer();
        }

        // Validate credentials
        lock_guard<mutex> avnGuard(avnListMutex);
        for (const auto& avn : avnList) 
        {
            if (avn.flightNumber == flightNumber && avn.avnID == avnID) 
            {
                airlineName = avn.airlineName;
                cout << "Login successful for " << airlineName << "!\n";
                return true;
            }
        }

        attempts--;
        cout << "Invalid credentials. " << attempts << " attempts remaining.\n";
    }

    cout << "Too many failed attempts. Returning to main menu...\n";
    return false;
}

void displayAirlineMenu() 
{
    lock_guard<mutex> guard(coutMutex);
    cout << "\n=== Airline Portal Menu ===\n";
    cout << "1. View All AVNs\n";
    cout << "2. View Specific AVN\n";
    cout << "3. Initiate Payment\n";
    cout << "4. Logout\n";
}

void viewAllAVNs(const string& airlineName) 
{
    // lock_guard<mutex> guard(coutMutex);
    // lock_guard<mutex> avnGuard(avnListMutex);

    cout << "\n=== AVNs for " << airlineName << " ===\n";

    // Column widths
    const int idWidth = 15;
    const int flightWidth = 12;
    const int typeWidth = 12;
    const int speedWidth = 15;
    const int altWidth = 15;
    const int fineWidth = 15;
    const int statusWidth = 10;
    const int timeWidth = 20;

    // Table header
    cout << "| " << left << setw(idWidth) << "AVN ID"
        << "| " << left << setw(flightWidth) << "Flight No"
        << "| " << left << setw(typeWidth) << "Type"
        << "| " << right << setw(speedWidth) << "Speed (km/h)"
        << "| " << right << setw(altWidth) << "Altitude (ft)"
        << "| " << right << setw(fineWidth) << "Fine (PKR)"
        << "| " << left << setw(statusWidth) << "Status"
        << "| " << left << setw(timeWidth) << "Issue Time"
        << "| " << left << setw(timeWidth) << "Due Time" << " |\n";
    cout << "+-" << string(idWidth, '-') << "-+-"
        << string(flightWidth, '-') << "-+-"
        << string(typeWidth, '-') << "-+-"
        << string(speedWidth, '-') << "-+-"
        << string(altWidth, '-') << "-+-"
        << string(fineWidth, '-') << "-+-"
        << string(statusWidth, '-') << "-+-"
        << string(timeWidth, '-') << "-+-"
        << string(timeWidth, '-') << "-+\n";

    // Table rows
    bool hasAVNs = false;
    time_t now = time(nullptr);
    for (const auto& avn : avnList) 
    {
        if (avn.airlineName == airlineName) 
        {
            hasAVNs = true;
            string status = avn.paymentStatus;
            if (status == "unpaid" && difftime(now, avn.dueDate) > 0) 
                status = "overdue";

            char issueTimeStr[20];
            strftime(issueTimeStr, sizeof(issueTimeStr), "%Y-%m-%d %H:%M", localtime(&avn.issueTime));
            char dueTimeStr[20];
            strftime(dueTimeStr, sizeof(dueTimeStr), "%Y-%m-%d %H:%M", localtime(&avn.dueDate));
            cout << "| " << left << setw(idWidth) << avn.avnID
                << "| " << left << setw(flightWidth) << avn.flightNumber
                << "| " << left << setw(typeWidth) << getAircraftTypeName(avn.aircraftType)
                << "| " << right << setw(speedWidth) << fixed << setprecision(2) << avn.speedRecorded << " (" << avn.speedPermissible << ")"
                << "| " << right << setw(altWidth) << fixed << setprecision(2) << avn.altitudeRecorded << " (" << avn.altitudePermissible << ")"
                << "| " << right << setw(fineWidth) << fixed << setprecision(2) << avn.fineAmount
                << "| " << left << setw(statusWidth) << status
                << "| " << left << setw(timeWidth) << issueTimeStr
                << "| " << left << setw(timeWidth) << dueTimeStr << " |\n";
        }
    }

    // Table footer
    cout << "+-" << string(idWidth, '-') << "-+-"
        << string(flightWidth, '-') << "-+-"
        << string(typeWidth, '-') << "-+-"
        << string(speedWidth, '-') << "-+-"
        << string(altWidth, '-') << "-+-"
        << string(fineWidth, '-') << "-+-"
        << string(statusWidth, '-') << "-+-"
        << string(timeWidth, '-') << "-+-"
        << string(timeWidth, '-') << "-+\n";

    if (!hasAVNs) 
        cout << "No AVNs found for " << airlineName << ".\n";

    cout << "Press Enter to continue...";
    clearInputBuffer();
    cin.get();
}

void viewSpecificAVN(const string& airlineName) 
{
    //lock_guard<mutex> guard(coutMutex);
    string avnID;
    cout << "\n=== View Specific AVN ===\n";
    
    while (true) 
    {
        cout << "Enter AVN ID (e.g., AVN-20250507-001): ";
        getline(cin, avnID);
        regex avnRegex("AVN-\\d{8}-\\d{3}");

        if (regex_match(avnID, avnRegex))
            break;
        
        cout << "Invalid AVN ID format. Use AVN-YYYYMMDD-NNN. Press Enter to continue...\n";
        clearInputBuffer();
    }

    //lock_guard<mutex> avnGuard(avnListMutex);
    bool found = false;
    time_t now = time(nullptr);
    for (const auto& avn : avnList) 
    {
        if (avn.airlineName == airlineName && avn.avnID == avnID) 
        {
            found = true;
            string status = avn.paymentStatus;
            if (status == "unpaid" && difftime(now, avn.dueDate) > 0) 
                status = "overdue";
            
            char issueTimeStr[20];
            strftime(issueTimeStr, sizeof(issueTimeStr), "%Y-%m-%d %H:%M", localtime(&avn.issueTime));
            char dueTimeStr[20];
            strftime(dueTimeStr, sizeof(dueTimeStr), "%Y-%m-%d %H:%M", localtime(&avn.dueDate));
            cout << "\nAVN Details:\n";
            cout << "AVN ID: " << avn.avnID << "\n";
            cout << "Flight Number: " << avn.flightNumber << "\n";
            cout << "Airline: " << avn.airlineName << "\n";
            cout << "Aircraft Type: " << getAircraftTypeName(avn.aircraftType) << "\n";
            cout << "Speed Recorded: " << fixed << setprecision(2) << avn.speedRecorded << " km/h (" << avn.speedPermissible << ")\n";
            cout << "Altitude Recorded: " << fixed << setprecision(2) << avn.altitudeRecorded << " ft (" << avn.altitudePermissible << ")\n";
            cout << "Fine Amount: PKR " << fixed << setprecision(2) << avn.fineAmount << "\n";
            cout << "Payment Status: " << status << "\n";
            cout << "Issue Time: " << issueTimeStr << "\n";
            cout << "Due Time: " << dueTimeStr << "\n";
            break;
        }
    }

    if (!found) 
        cout << "AVN ID " << avnID << " not found for " << airlineName << "." << endl;
    
    cout << "Press Enter to continue...";
    clearInputBuffer();
    cin.get();
}

void initiatePayment(const string& airlineName) 
{
    //lock_guard<mutex> guard(coutMutex);
    string avnID;
    cout << "\n=== Initiate Payment ===\n";
    while (true) 
    {
        cout << "Enter AVN ID for payment (e.g., AVN-20250507-001): ";
        getline(cin, avnID);
        regex avnRegex("AVN-\\d{8}-\\d{3}");
        
        if (regex_match(avnID, avnRegex)) 
            break;
        
        cout << "Invalid AVN ID format. Use AVN-YYYYMMDD-NNN. Press Enter to continue...\n";
        clearInputBuffer();
    }

    //lock_guard<mutex> avnGuard(avnListMutex);
    for (const auto& avn : avnList) 
    {
        if (avn.airlineName == airlineName && avn.avnID == avnID) 
        {
            if (avn.paymentStatus == "paid") 
                cout << "AVN " << avnID << " is already paid.\n";
            else 
            {
                mockStripePay(avn);
                return;
            }
        }
    }
    cout << "AVN ID " << avnID << " not found or not associated with " << airlineName << ".\n";
    cout << "Press Enter to continue...";
    clearInputBuffer();
    cin.get();
}

void mockStripePay(const AVN& avn)
{
    //lock_guard<mutex> guard(coutMutex);
    cout << "\n=== StripePay Payment Process ===\n";
    cout << "AVN ID: " << avn.avnID << "\n";
    cout << "Flight Number: " << avn.flightNumber << "\n";
    cout << "Airline: " << avn.airlineName << "\n";
    cout << "Fine Amount: PKR " << fixed << setprecision(2) << avn.fineAmount << "\n";

    string cardNumber, expiry, cvv, amountStr;
    int amount;

    // Card no. validation:
    while (true == true) 
    {
        cout << "Enter Card Number (16 digits): ";
        getline(cin, cardNumber);
        regex cardRegex("\\d{16}");
        
        if (regex_match(cardNumber, cardRegex)) 
            break;
        
        cout << "Invalid card number. Must be 16 digits. Press Enter to continue...\n";
        clearInputBuffer();
    }

    // expiry dsate validation:
    while (1 == 1) 
    {
        cout << "Enter Expiry (MM/YY): ";
        getline(cin, expiry);
        regex expiryRegex("\\d{2}/\\d{2}");

        if (regex_match(expiry, expiryRegex))
            break;
        
        cout << "Invalid expiry format. Use MM/YY. Press Enter to continue...\n";
        clearInputBuffer();
    }

    // Dummy CVV
    while (true) 
    {
        cout << "Enter CVV (3 digits): ";
        getline(cin, cvv);
        regex cvvRegex("\\d{3}");
        
        if (regex_match(cvv, cvvRegex)) 
            break;
        
        cout << "Invalid CVV. Must be 3 digits. Press Enter to continue...\n";
        clearInputBuffer();
    }

    // Amount validation
    while (true) 
    {
        cout << "Enter Amount to Pay (PKR): ";
        getline(cin, amountStr);
        try 
        {
            amount = stoi(amountStr);
            
            if (amount == avn.fineAmount) 
                break;
            
            cout << "Amount must match fine: PKR " << avn.fineAmount << "\nYou entered: " << amount << "\nPress Enter to continue...\n";
        }
        catch (...) 
        {
            cout << "Invalid amount. Enter a valid number. Press Enter to continue...\n";
        }
        clearInputBuffer();
    }

    // Simulate payment processing
    cout << "Processing payment...\n";
    this_thread::sleep_for(chrono::seconds(2));
    string confirmationID = "STRIPE-" + to_string(rand() % 1000000);
    cout << "Payment successful! Confirmation ID: " << confirmationID << "\n";

    // Update payment status
    updateAVNPaymentStatus(avn.avnID);
    cout << "AVN status updated to 'paid'. Press Enter to continue...\n";
    clearInputBuffer();
    cin.get();
}

void spawnCustomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) 
{
    //lock_guard<mutex> guard(coutMutex);
    cout << "\n=== Spawn Custom Aircraft ===\n";

    // Display available airlines
    cout << "Available Airlines:\n";
    for (size_t i = 0; i < airlines.size(); ++i) 
        cout << i + 1 << ". " << airlines[i].name << " (" << getAircraftTypeName(airlines[i].type) << ")\n";
    

    // Get airline choice
    int airlineChoice;
	string input;
    Airline* selectedAirline = nullptr;
    bool validAirline = false;
    while (!validAirline) 
    {
        cout << "Select airline (1-" << airlines.size() << "): ";
        getline(cin, input);

        if (!isValidInteger(input, airlineChoice, 1, airlines.size())) 
        {
            cout << "Invalid airline selection. Please try again. Press Enter to continue...\n";
			clearInputBuffer();
            continue;
        }

        selectedAirline = &airlines[airlineChoice - 1];
        if (selectedAirline->type == CARGO && cargoCreated) 
            cout << "Error: Only one cargo flight is allowed per simulation. Please select a different airline.\n";
        else
            validAirline = true;
    }

    string flightNumber;
    while (true)
    {
        cout << "Enter flight number (e.g., PK101): ";
        getline(cin, flightNumber);
        if (flightNumber.empty() || flightNumber.length() > 10) 
        {
            cout << "Invalid flight number. Must be non-empty and less than 10 characters. Press Enter to continue...\n";
            clearInputBuffer();
            continue;
        }
        // Check if flight number is unique
        bool isUnique = true;
        for (const auto* aircraft : activeAircrafts) 
        {
            if (aircraft->flightNumber == flightNumber) 
            {
                isUnique = false;
                break;
            }
        }

        if (!isUnique) 
        {
            cout << "Error: Flight number " << flightNumber << " is already in use. Please enter a unique flight number. Press Enter to continue...\n";
            clearInputBuffer();
            continue;
        }
        break;
    }

    // Get direction
    int dirChoice;
    while (true) 
    {
        cout << "Select direction (1. North, 2. South, 3. East, 4. West): ";
        getline(cin, input);
        
        if (isValidInteger(input, dirChoice, 1, 4)) 
            break;
        
        cout << "Invalid input. Please enter a number between 1 and 4. Press Enter to continue...\n";
        clearInputBuffer();
    }

    Direction direction;
    switch (dirChoice) 
    {
        case 1: direction = NORTH; break;
        case 2: direction = SOUTH; break;
        case 3: direction = EAST; break;
        case 4: direction = WEST; break;
    }

    int priChoice;
    while (true) 
    {
        cout << "Select priority (1. Emergency, 2. High, 3. Normal): ";
        getline(cin, input);
        
        if (isValidInteger(input, priChoice, 1, 3)) 
            break;
        
        cout << "Invalid input. Please enter a number between 1 and 3. Press Enter to continue...\n";
        clearInputBuffer();
    }

    Priority priority;
    switch (priChoice) 
    {
        case 1: priority = EMERGENCY_PRIORITY; break;
        case 2: priority = HIGH_PRIORITY; break;
        case 3: priority = NORMAL_PRIORITY; break;
    }

    string schedTimeStr;
    struct tm tm = {};
    time_t scheduledTime;
    while (true) {
        cout << "Enter scheduled time (YYYY-MM-DD HH:MM) or 'now' for current time: ";
        getline(cin, schedTimeStr);
        
        // Checking for NOW:
        string lowerInput = schedTimeStr;
        transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        if (lowerInput == "now") 
        {
            scheduledTime = time(nullptr);
            break;
        }

        // Validation onto time format
        if (isValidTimeFormat(schedTimeStr, tm)) 
        {
            scheduledTime = mktime(&tm);
            break;
        }
        cout << "Invalid input. Use YYYY-MM-DD HH:MM with valid ranges (e.g., year >= 2025, hours 0-23) or 'now'.\nPress Enter to continue...\n";
        clearInputBuffer();
    }

    // Creating the aircraft:
    Aircraft* newAircraft = createAircraftForManualEntry(airlines, direction, ++aircraftSequence, flightNumber, selectedAirline->type, priority, scheduledTime);
    activeAircrafts.push_back(newAircraft);

    // Updating cargoCreated, remember 1 cargo per simulation:
    if (selectedAirline->type == CARGO) 
        cargoCreated = true;

    cout << "Flight " << flightNumber << " spawned successfully.\n";
}



void spawnRandomAircraft(vector<Airline>& airlines, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) 
{
    //lock_guard<mutex> guard(coutMutex);
    cout << "\n=== Spawn Random Aircraft ===\n";

    // Randomly select direction
    Direction direction = static_cast<Direction>(rand() % 4); // NORTH, SOUTH, EAST, WEST

    // Create aircraft
    Aircraft* newAircraft = createAircraftForAutoEntry(airlines, direction, ++aircraftSequence);
    activeAircrafts.push_back(newAircraft);

    cout << "Random flight " << newAircraft->flightNumber << " (" << newAircraft->airline->name
         << ") spawned from " << getDirectionName(direction) << " direction.\n";
}


void startSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) 
{
    lock_guard<mutex> guard(simulationMutex);
    if (!simulationRunning) {
        simulationRunning = true;
        simulationPaused = true; // Start paused
        simulationThread = thread(runSimulation, ref(airlines), ref(runways), ref(activeAircrafts), ref(aircraftSequence));
        simulationThread.detach();
    }
}

void runSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) 
{
    while (simulationRunning && globalElapsedTime < SIMULATION_DURATION) 
        this_thread::sleep_for(chrono::milliseconds(100));

    lock_guard<mutex> guard(simulationMutex);
    if (simulationRunning) 
    {
        simulationRunning = false;
        simulationPaused = true;
        lock_guard<mutex> coutGuard(coutMutex);
        cout << "\nSimulation completed.\n";
        displayAVNStatistics(airlines);
    }
}

// New visualization function
void visualizeSimulation(sf::RenderWindow& window, const vector<Aircraft*>& aircrafts, const vector<Runway>& runways, int elapsedTime) 
{
    window.clear(sf::Color::Black);

    // Font for text
    sf::Font font;
    if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) 
    {
        cout << "Error: Could not load font\n";
        return;
    }

    // Draw runways
    vector<sf::RectangleShape> runwayShapes(3);
    for (size_t i = 0; i < runways.size(); ++i) 
    {
        runwayShapes[i].setSize(sf::Vector2f(400, 30));
        runwayShapes[i].setPosition(50, 50 + i * 100);
        runwayShapes[i].setFillColor(runways[i].isOccupied ? sf::Color::Red : sf::Color::Green);

        // Runway label
        sf::Text label(runways[i].name, font, 14);
        label.setPosition(10, 50 + i * 100);
        label.setFillColor(sf::Color::White);
        window.draw(label);

        // Waiting queue count
        sf::Text queueText("Waiting: " + to_string(runways[i].waitingQueue.size()), font, 14);
        queueText.setPosition(460, 50 + i * 100);
        queueText.setFillColor(sf::Color::White);
        window.draw(queueText);

        window.draw(runwayShapes[i]);
    }

    // Draw aircraft with unique positioning and smooth transitions
    for (const auto& aircraft : aircrafts) 
    {
        if (aircraft->isFaulty) continue;

        sf::RectangleShape aircraftShape(sf::Vector2f(20, 20));
        switch (aircraft->priority) 
        {
            case EMERGENCY_PRIORITY: aircraftShape.setFillColor(sf::Color::Red); break;
            case HIGH_PRIORITY: aircraftShape.setFillColor(sf::Color::Yellow); break;
            case NORMAL_PRIORITY: aircraftShape.setFillColor(sf::Color::Blue); break;
        }

        float x = 50, y = 0; // iniitial position
        float progress = static_cast<float>(aircraft->timeInFlight) / 25.0f; // 25s max phase duration

        
        if (aircraft->phase == HOLDING || aircraft->phase == APPROACH) 
        {
            // Unique circular path per aircraft based on flightNumber hash
            size_t hash = std::hash<std::string>{}(aircraft->flightNumber) % 3;  
            float angle = aircraft->timeInFlight * 0.1f + hash * 2.0f;        
            x = 600 + cos(angle) * (50 + hash * 20);                  // radii variation
            y = 100 + sin(angle) * (50 + hash * 20);
        } 

        else if (aircraft->phase == LANDING || aircraft->phase == TAXI) 
        {
            for (size_t i = 0; i < runways.size(); ++i) 
            {
                if (runways[i].currentAircraft == aircraft && (runways[i].id == RWY_A || runways[i].id == RWY_C)) 
                {
                    y = 50 + i * 100 + 5;
                    x = 50 + (progress * 400.0f); // Full 400px runway traversal
                    if (x > 450) x = 450; // Cap at runway end
                    break;
                }
            }
        } 
        else if (aircraft->phase == TAKEOFF_ROLL || aircraft->phase == CLIMB) 
        {
            for (size_t i = 0; i < runways.size(); ++i) 
            {
                if (runways[i].currentAircraft == aircraft && (runways[i].id == RWY_B || runways[i].id == RWY_C)) 
                {
                    y = 50 + i * 100 + 5;
                    x = 50 + (progress * 400.0f); // Full 400px runway traversal
                    if (x > 450) x = 450; // Cap at runway end
                    break;
                }
            }
        } 
        else if (aircraft->phase == AT_GATE || aircraft->phase == AT_GATE_BRS) 
        {
            size_t hash = std::hash<std::string>{}(aircraft->flightNumber) % 5; // Spread gates horizontally
            x = 50 + (hash * 50); // Spread gates horizontally
            y = 350;
        } 
        else 
        {
            x = 600; // Off-screen for cruising
            y = 100;
        }

        aircraftShape.setPosition(x, y);
        window.draw(aircraftShape);

        // Aircraft status text with offset to avoid overlap
        size_t hash = std::hash<std::string>{}(aircraft->flightNumber) % 3; // Use hash for text offset
        string statusText = aircraft->flightNumber + ": " + aircraft->status + " (Fuel: " +
                            to_string(aircraft->fuelStatus) + "%, Alt: " + to_string((int)aircraft->altitude) + "m)";
        sf::Text aircraftText(statusText, font, 12);
        aircraftText.setPosition(x, y + 25 + (hash * 15)); // Vertical offset per aircraft
        aircraftText.setFillColor(sf::Color::White);
        window.draw(aircraftText);
    }

    // Status panel
    int minutes = elapsedTime / 60;
    int seconds = elapsedTime % 60;
    string timeText = "Time: " + to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + to_string(seconds);
    sf::Text statusText(timeText + "\nArrivals: " + to_string(arrivalQueue.size()) +
                        "\nDepartures: " + to_string(departureQueue.size()), font, 14);
    statusText.setPosition(10, 400);
    statusText.setFillColor(sf::Color::White);
    window.draw(statusText);

    window.display();
}


void showSimulation(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts, int& aircraftSequence) 
{
    setNonBlockingInput(true);
    {
        lock_guard<mutex> guard(coutMutex);
        cout << "Showing simulation. Press 'q' to return to menu.\n" << flush;
    }

    // Create SFML window
    sf::RenderWindow window(sf::VideoMode(800, 600), "AirControlX Simulation");
    window.setFramerateLimit(60);

    auto startTime = chrono::steady_clock::now();
    while (simulationRunning && globalElapsedTime < SIMULATION_DURATION && window.isOpen()) 
    {
        // Handle SFML events
        sf::Event event;
        while (window.pollEvent(event)) 
        {
            if (event.type == sf::Event::Closed || (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Q)) 
                window.close();
        }

        // GUARDING THE FOLLOWING SCOPE:
        {
            lock_guard<mutex> guard(simulationMutex);
            if (kbhit()) 
            {
                cout << "Returning to menu.\n" << flush;
                simulationPaused = true;
                window.close();
                break;
            }
            simulationPaused = false;

            // Update simulation
            updateSimulationStep(airlines, runways, activeAircrafts);
            globalElapsedTime += TIME_STEP;

            // Terminal output
            displayStatus(activeAircrafts, runways, globalElapsedTime);

            // SFML visualization
            visualizeSimulation(window, activeAircrafts, runways, globalElapsedTime);
        }

        this_thread::sleep_for(chrono::milliseconds(TIME_STEP * 1000));

        
        auto elapsed = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime).count();
        
        // 30s timeout:
        if (elapsed >= 30) 
        {
            lock_guard<mutex> guard(coutMutex);
            cout << "Timeout: Returning to menu.\n" << flush;
            lock_guard<mutex> guard2(simulationMutex);
            simulationPaused = true;
            window.close();
            break;
        }
    }

    setNonBlockingInput(false);
}

void updateSimulationStep(vector<Airline>& airlines, vector<Runway>& runways, vector<Aircraft*>& activeAircrafts) 
{
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

    // Random ground faults every 30 seconds are handled:
    if (globalElapsedTime % 30 == 0)
        handleGroundFaults(activeAircrafts);

    // Checking for aircrafts to be removed
    if (globalElapsedTime % 7 == 0 && globalElapsedTime != 0) 
    {
        for (auto it = activeAircrafts.begin(); it != activeAircrafts.end(); ) 
        {
            if ((*it)->phase == AT_GATE_BRS || (*it)->phase == DEPARTURE_CRUISE_BRS)
                removeAircraftFromEverywhere(*it, activeAircrafts, runways);
            else
                it++;
        }
    }

    // Check for violations
    for (auto aircraft : activeAircrafts) 
        checkForViolations(*aircraft);

    // Update time in flight for each aircraft
    for (auto& aircraft : activeAircrafts) 
        if (!isAircraftWaiting(*aircraft, runways)) 
            aircraft->timeInFlight++;
    
}

// Detecting a key press (detecting 'q' to exit simulation and enter menu)
bool kbhit() 
{
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        lock_guard<mutex> guard(coutMutex);
        cout << "Key detected: " << c << "\n" << flush;
        return c == 'q';
    }
    return false;
}

void initializeAirlines(vector<Airline>& airlines) 
{
    airlines = 
    {
        {"PIA", COMMERCIAL, 6, 4, 0},
        {"AirBlue", COMMERCIAL, 4, 4, 0},
        {"FedEx", CARGO, 3, 2, 0},
        {"Pakistan Airforce", EMERGENCY, 2, 1, 0},
        {"Blue Dart", CARGO, 2, 2, 0},
        {"AghaKhan Air Ambulance", EMERGENCY, 2, 1, 0}
    };
}

void initializeRunways(vector<Runway>& runways) 
{
    runways = 
    {
        {RWY_A, "RWY-A (North-South Arrivals)", false, nullptr},
        {RWY_B, "RWY-B (East-West Departures)", false, nullptr},
        {RWY_C, "RWY-C (Cargo/Emergency/Overflow)", false, nullptr}
    };
}

string generateFlightNumber(const Airline& airline, int sequence) 
{
    string prefix;
    if (airline.name == "PIA") prefix = "PK";
    else if (airline.name == "AirBlue") prefix = "PA";
    else if (airline.name == "FedEx") prefix = "FX";
    else if (airline.name == "Pakistan Airforce") prefix = "PF";
    else if (airline.name == "Blue Dart") prefix = "BD";
    else if (airline.name == "AghaKhan Air Ambulance") prefix = "AK";

    return prefix + to_string(100 + sequence);
}

Aircraft* createAircraftForManualEntry(vector<Airline>& airlines, Direction direction, int seq, string flightNum, AircraftType type, Priority priority, time_t schedTime) 
{
    Aircraft* aircraft = new Aircraft();

    // Select airline based on type
    Airline* selectedAirline = nullptr;
    for (auto& airline : airlines) 
    {
        if (airline.type == type) 
        {
            selectedAirline = &airline;
            break;
        }
    }

    if (!selectedAirline) selectedAirline = &airlines[0];

    aircraft->flightNumber = flightNum;
    aircraft->airline = selectedAirline;
    aircraft->type = selectedAirline->type;
    aircraft->direction = direction;
    aircraft->isEmergency = (priority == EMERGENCY_PRIORITY);
    aircraft->hasAVN = false;
    aircraft->isFaulty = false;
    aircraft->fuelStatus = 20 + rand() % 60;
    aircraft->scheduledTime = schedTime;
    aircraft->priority = priority;

    // Set initial phase, speed, and altitude based on direction
    if (direction == NORTH || direction == SOUTH) 
    {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        aircraft->altitude = 19000 + (rand() % 1000); // 19,000-20,000 ft
        arrivalQueue.push(aircraft);
    } 
    else 
    {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        aircraft->altitude = 0; // On ground
        departureQueue.push(aircraft);
    }

    // Add to status map
    aircraftStatusMap[aircraft->flightNumber] = aircraft;

    return aircraft;
}

Aircraft* createAircraftForAutoEntry(const vector<Airline>& airlines, Direction direction, int seq) 
{
    Aircraft* aircraft = new Aircraft();

    // Determine if this is an emergency flight based on direction probabilities
    bool isEmergency = false;
    int emergencyProb = 0;

    switch (direction) 
    {
        case NORTH: emergencyProb = 10; break; // 10%
        case SOUTH: emergencyProb = 5; break;  // 5%
        case EAST: emergencyProb = 15; break; // 15%
        case WEST: emergencyProb = 20; break; // 20%
    }

    isEmergency = (rand() % 100) < emergencyProb;

    // Selecting whichever airline based on needed type of aircraft:
    vector<const Airline*> emergencyEligibleAirlines;
    for (const auto& airline : airlines) 
    {
        if (isEmergency && (airline.type == EMERGENCY))
            emergencyEligibleAirlines.push_back(&airline);
        else if (!isEmergency && (airline.type != EMERGENCY))
            emergencyEligibleAirlines.push_back(&airline);
    }

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

    if (eligibleAirlines.empty()) 
        eligibleAirlines.push_back(&airlines[0]);
    

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
    aircraft->fuelStatus = 20 + rand() % 60;
    aircraft->scheduledTime = time(nullptr);

    if (aircraft->isEmergency || aircraft->type == EMERGENCY) 
        aircraft->priority = EMERGENCY_PRIORITY;
    else if (aircraft->type == CARGO || aircraft->fuelStatus < 30)
        aircraft->priority = HIGH_PRIORITY;
    else 
        aircraft->priority = NORMAL_PRIORITY;
    
    // ARRIVALS;
    if (direction == NORTH || direction == SOUTH) 
    {
        aircraft->phase = HOLDING;
        aircraft->speed = 555 + rand() % 10; // 555-564 km/h
        aircraft->altitude = 19000 + (rand() % 1000); // 19,000-20,000 ft
        arrivalQueue.push(aircraft);
        // Rebuild queue if priority is HIGH_PRIORITY due to low fuel
        if (aircraft->fuelStatus < 30 && aircraft->priority == HIGH_PRIORITY) 
        {
            lock_guard<mutex> guard(coutMutex);
            cout << "Flight " << aircraft->flightNumber << " upgraded to HIGH_PRIORITY due to low fuel.\n";
            rebuildQueue(aircraft, arrivalQueue);
        }
    } 

    // DEPARTURES:
    else 
    {
        aircraft->phase = AT_GATE;
        aircraft->speed = 0;
        aircraft->altitude = 0;
        departureQueue.push(aircraft);

        // Rebuilding the  queue if priority is HIGH_PRIORITY due to low fuel:
        if (aircraft->fuelStatus < 30 && aircraft->priority == HIGH_PRIORITY) 
        {
            lock_guard<mutex> guard(coutMutex);
            cout << "Flight " << aircraft->flightNumber << " upgraded to HIGH_PRIORITY due to low fuel.\n";
            rebuildQueue(aircraft, departureQueue);
        }
    }



    aircraftStatusMap[aircraft->flightNumber] = aircraft;
    return aircraft;
}




void simulateArrival(Aircraft& aircraft, vector<Runway>& runways) 
{
    // Skip phase transitions if aircraft is waiting for a runway
    if (isAircraftWaiting(aircraft, runways)) 
        return;

    switch (aircraft.phase) 
    {
        case HOLDING:
            if (aircraft.timeInFlight >= 5) 
            {
                aircraft.phase = APPROACH;
                aircraft.status = "Approaching";
                aircraft.fuelStatus -= 2;
            }
            break;

        case APPROACH: 
        {
            Runway* assignedRunway = assignRunway(aircraft, runways);
            if (assignedRunway && aircraft.timeInFlight >= 9) 
            {
                aircraft.phase = LANDING;
                aircraft.status = "Landing";
                aircraft.fuelStatus -= 2;
            }
            break;
        }

        case LANDING:
            if (aircraft.timeInFlight >= 14) 
            {
                aircraft.phase = TAXI;
                aircraft.status = "Taxiing";
                aircraft.fuelStatus -= 2;

                // Free the runway and assign next aircraft
                for (auto& runway : runways) 
                {
                    if (runway.currentAircraft == &aircraft) 
                    {
                        runway.isOccupied = false;
                        runway.currentAircraft = nullptr;

                        // Assign next aircraft from queue if available
                        if (!runway.waitingQueue.empty()) 
                        {
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
            if (aircraft.timeInFlight >= 17) 
            {
                aircraft.phase = AT_GATE;
                aircraft.status = "At Gate";
                aircraft.fuelStatus -= 2;
            }
            break;

        case AT_GATE:
            if (aircraft.timeInFlight >= 20 && aircraft.speed <= 0) 
            {
                aircraft.phase = AT_GATE_BRS;
                aircraft.status = "Completed";
                aircraft.fuelStatus -= 1;
            }
            break;

        default:
            break;
    }
}

void simulateDeparture(Aircraft& aircraft, vector<Runway>& runways) 
{
    // No need to change transitions since aircraft waiting:
    if (isAircraftWaiting(aircraft, runways)) 
        return;    

    switch (aircraft.phase) 
    {
        case AT_GATE:
            if (aircraft.timeInFlight >= 3) 
            {
                aircraft.phase = TAXI;
                aircraft.status = "Taxiing";
                aircraft.fuelStatus -= 2;
            }
            break;

        case TAXI: 
        {
            Runway* assignedRunway = assignRunway(aircraft, runways);
            if (assignedRunway && aircraft.timeInFlight >= 6) 
            {
                aircraft.phase = TAKEOFF_ROLL;
                aircraft.status = "Taking Off";
                aircraft.fuelStatus -= 2;
            }
            break;
        }

        case TAKEOFF_ROLL:
            if (aircraft.timeInFlight >= 12) 
            {
                aircraft.phase = CLIMB;
                aircraft.status = "Climbing";
                aircraft.fuelStatus -= 2;
                
                // Freeing the runway and assigning next aircraft
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
            if (aircraft.timeInFlight >= 16) 
            {
                aircraft.phase = ACCELERATING_TO_CRUISE;
                aircraft.status = "Accelerating";
                aircraft.fuelStatus -= 2;
            }
            break;

        case ACCELERATING_TO_CRUISE:
            if (aircraft.timeInFlight >= 23) 
            {
                aircraft.phase = DEPARTURE_CRUISE;
                aircraft.status = "Cruising";
                aircraft.fuelStatus -= 3;
            }
            break;

        case DEPARTURE_CRUISE:
            if (aircraft.timeInFlight >= 25) 
            {
                aircraft.phase = DEPARTURE_CRUISE_BRS;
                aircraft.status = "Completed";
                aircraft.fuelStatus -= 4;
            }
            break;

        default:
            break;
    }
}

Runway* assignRunway(Aircraft& aircraft, vector<Runway>& runways) 
{
    // If already assigned:
    for (auto& runway : runways) 
    {
        if (runway.currentAircraft == &aircraft) 
            return &runway;        
    }

    // if aircraft is in a waiting queue
    for (auto& runway : runways) 
    {
        queue<Aircraft*> tempQueue = runway.waitingQueue;
        while (!tempQueue.empty()) 
        {
            Aircraft* queuedAircraft = tempQueue.front();
            tempQueue.pop();
            if (queuedAircraft == &aircraft)
                return &runway;
        }
    }

    // Assign runway based on aircraft type and direction
    Runway* assignedRunway = nullptr;
    if (aircraft.priority == EMERGENCY_PRIORITY) 
    {
        if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
        else if (!runways[RWY_A].isOccupied) assignedRunway = &runways[RWY_A];
        else if (!runways[RWY_B].isOccupied) assignedRunway = &runways[RWY_B];
        else 
        {
            assignedRunway = &runways[RWY_C];
            for (auto& runway : runways) 
            {
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

    if (aircraft.type == CARGO) 
    {
        if (!runways[RWY_C].isOccupied) 
        {
            assignedRunway = &runways[RWY_C];
            assignedRunway->isOccupied = true;
            assignedRunway->currentAircraft = &aircraft;
            return assignedRunway;
        } 
        else 
        {
            runways[RWY_C].waitingQueue.push(&aircraft);
            aircraft.status = "Waiting for Runway";
            return nullptr;
        }
    }

    if (aircraft.direction == NORTH || aircraft.direction == SOUTH) 
    {
        if (!runways[RWY_A].isOccupied) assignedRunway = &runways[RWY_A];
        else if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
        else 
        {
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
    else 
    {
        if (!runways[RWY_B].isOccupied) assignedRunway = &runways[RWY_B];
        else if (!runways[RWY_C].isOccupied) assignedRunway = &runways[RWY_C];
        else 
        {
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

void updateWaitTimes(vector<Runway>& runways) 
{
    for (auto& pair : aircraftStatusMap) 
    {
        Aircraft* aircraft = pair.second;
        // Update actual wait time for aircraft in waiting queue
        if (aircraft->status == "Waiting for Runway") 
        {
            aircraft->actualWaitTime++;
            // Aging: Upgrade NORMAL_PRIORITY to HIGH_PRIORITY after AGING_THRESHOLD
            if (aircraft->actualWaitTime >= AGING_THRESHOLD && aircraft->priority == NORMAL_PRIORITY) 
            {
                aircraft->priority = HIGH_PRIORITY;
                lock_guard<mutex> guard(coutMutex);
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

int calculateEstimatedWaitTime(Aircraft& aircraft, const vector<Runway>& runways) 
{
    // If aircraft is past runway phases, no wait time
    if ((aircraft.direction == NORTH || aircraft.direction == SOUTH) &&
        (aircraft.phase == LANDING || aircraft.phase == TAXI || aircraft.phase == AT_GATE || aircraft.phase == AT_GATE_BRS)) 
        return 0;
    
    if ((aircraft.direction == EAST || aircraft.direction == WEST) &&
        (aircraft.phase == TAKEOFF_ROLL || aircraft.phase == CLIMB || aircraft.phase == ACCELERATING_TO_CRUISE ||
         aircraft.phase == DEPARTURE_CRUISE || aircraft.phase == DEPARTURE_CRUISE_BRS)) 
        return 0;
    

    int estimatedWait = 0;

    // Check if aircraft is in a waiting queue
    if (isAircraftWaiting(aircraft, runways)) 
    {
        // Find the runway and queue position
        for (const auto& runway : runways) 
        {
            queue<Aircraft*> tempQueue = runway.waitingQueue;
            int position = 0;
            bool found = false;
            vector<Aircraft*> higherOrEqualPriority;

            // Collect aircraft in queue
            while (!tempQueue.empty()) 
            {
                Aircraft* queuedAircraft = tempQueue.front();
                tempQueue.pop();
                
                if (queuedAircraft == &aircraft) 
                    found = true;
                
                if (queuedAircraft->priority <= aircraft.priority)  // Higher or equal priority
                    higherOrEqualPriority.push_back(queuedAircraft);
            }

            if (found) 
            {
                // Sorting by scheduled time for FCFS within priority
                sort(higherOrEqualPriority.begin(), higherOrEqualPriority.end(),
                     [](Aircraft* a, Aircraft* b) { return a->scheduledTime < b->scheduledTime; });

                // Finding position of current aircraft
                for (size_t i = 0; i < higherOrEqualPriority.size(); ++i) 
                {
                    if (higherOrEqualPriority[i] == &aircraft) 
                    {
                        position = i;
                        break;
                    }
                }

                // Calculate wait time
                estimatedWait = position * AVG_RUNWAY_USAGE_TIME;
                
                if (runway.isOccupied && runway.currentAircraft)                     
                    estimatedWait += AVG_RUNWAY_USAGE_TIME;
                
                break;
            }
        }
    } 
    else 
    {
        // Estimate time to reach runway-requiring phase
        if (aircraft.direction == NORTH || aircraft.direction == SOUTH) 
        {
            if (aircraft.phase == HOLDING) 
                estimatedWait = 9; 
             
            else if (aircraft.phase == APPROACH) 
                estimatedWait = 4; 
            
        } 
        else 
        {
            if (aircraft.phase == AT_GATE) 
                estimatedWait = 6; 
            else if (aircraft.phase == TAXI) 
                estimatedWait = 3; 
        }

        // Add potential queue wait time
        priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> queueCopy =
            (aircraft.direction == NORTH || aircraft.direction == SOUTH) ? arrivalQueue : departureQueue;
        int aheadCount = 0;

        // Count aircraft with higher or equal priority
        while (!queueCopy.empty()) {
            Aircraft* queuedAircraft = queueCopy.top();
            queueCopy.pop();
            if (queuedAircraft != &aircraft && queuedAircraft->priority <= aircraft.priority) 
                aheadCount++;            
        }
        estimatedWait += aheadCount * AVG_RUNWAY_USAGE_TIME;

        // Add time for currently occupying aircraft
        for (const auto& runway : runways) 
        {
            if (runway.isOccupied && runway.currentAircraft &&
                ((aircraft.direction == NORTH || aircraft.direction == SOUTH) &&
                 (runway.id == RWY_A || runway.id == RWY_C)) ||
                ((aircraft.direction == EAST || aircraft.direction == WEST) &&
                 (runway.id == RWY_B || runway.id == RWY_C))) 
            {
                     estimatedWait += AVG_RUNWAY_USAGE_TIME;
            }
        }
    }

    return estimatedWait;
}

void updateAircraftStatus(Aircraft& aircraft) 
{
    // no need to update if in waiting queue:
    if (aircraft.status == "Waiting for Runway") 
        return;    

    switch (aircraft.phase) 
    {
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

int calculateSpeedChange(const Aircraft& aircraft)
{
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

double calculateAltitudeChange(const Aircraft& aircraft) 
{
    double altitudeChange;

    switch (aircraft.phase) 
    {
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
            altitudeChange = 3000 + rand() % 1000;
            break;
        default:
            altitudeChange = 0;
            break;
    }

    return altitudeChange;
}

void updateAircraftSpeed(Aircraft& aircraft) 
{
    if (aircraft.status == "Waiting for Runway") 
    {
        if (aircraft.direction == NORTH || aircraft.direction == SOUTH) 
            aircraft.speed = 240 + rand() % 51; 
        else 
            aircraft.speed = 0;        
        return;
    }

    int speedChange = calculateSpeedChange(aircraft);
    if (aircraft.direction == NORTH || aircraft.direction == SOUTH)
        aircraft.speed -= speedChange;
    else
        aircraft.speed += speedChange;

    if (aircraft.speed < 0) 
    {
        if (aircraft.phase != AT_GATE)
            aircraft.speed = (rand() % 10) + 10;
        else
            aircraft.speed = 0;
    }
}

void updateAircraftAltitude(Aircraft& aircraft) 
{    
    if (aircraft.status == "Waiting for Runway") 
    {
        if (aircraft.direction == NORTH || aircraft.direction == SOUTH) 
        {        
            double altitudeChange = (rand() % 101 - 50); // -50 to +50 ft/s
            aircraft.altitude += altitudeChange;
        } 
        else 
            aircraft.altitude = 0;        
        return;
    }

    double altitudeChange = calculateAltitudeChange(aircraft);
    if (aircraft.direction == NORTH || aircraft.direction == SOUTH)
        aircraft.altitude -= altitudeChange; 
    else
        aircraft.altitude += altitudeChange; 

    if (aircraft.altitude < 0) aircraft.altitude = aircraft.status == "LANDING" ? 100 + rand() % 11 : 0;
}

void generateAVN(Aircraft& aircraft, bool speedViolation, bool altitudeViolation) 
{
    AVN avn;
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y%m%d", tm);

    // Padding avnSequence to three digits
    ostringstream seqStream;
    seqStream << setw(3) << setfill('0') << ++avnSequence;
    avn.avnID = "AVN-" + string(dateStr) + "-" + seqStream.str();
    avn.flightNumber = aircraft.flightNumber;
    avn.airlineName = aircraft.airline->name;
    avn.aircraftType = aircraft.type;
    avn.speedRecorded = aircraft.speed;
    avn.altitudeRecorded = aircraft.altitude;
    avn.issueTime = now;
    avn.paymentStatus = "unpaid";
    avn.dueDate = now + DUE_DAYS * 24 * 60 * 60;
    double baseFine = (aircraft.type == CARGO) ? CARGO_FINE : COMMERCIAL_FINE;
    avn.fineAmount = baseFine * (1.0 + SERVICE_FEE);
    
    switch (aircraft.phase) 
    {
        case HOLDING:
            avn.speedPermissible = "<=600";
            avn.altitudePermissible = "9000-20000";
            break;
        case APPROACH:
            avn.speedPermissible = "240-290";
            avn.altitudePermissible = "3000-10250";
            break;
        case LANDING:
            avn.speedPermissible = "<=240";
            avn.altitudePermissible = "0-3100";
            break;
        case TAXI:
            avn.speedPermissible = "<=30";
            avn.altitudePermissible = "0";
            break;
        case AT_GATE:
            avn.speedPermissible = "<=10";
            avn.altitudePermissible = "0";
            break;
        case TAKEOFF_ROLL:
            avn.speedPermissible = "<=290";
            avn.altitudePermissible = "0-1000";
            break;
        case CLIMB:
            avn.speedPermissible = "<=463";
            avn.altitudePermissible = "900-20000";
            break;
        case ACCELERATING_TO_CRUISE:
            avn.speedPermissible = "N/A";
            avn.altitudePermissible = "N/A";
            break;
        case DEPARTURE_CRUISE:
            avn.speedPermissible = "800-900";
            avn.altitudePermissible = "29000-40000";
            break;
        default:
            avn.speedPermissible = "N/A";
            avn.altitudePermissible = "N/A";
            break;
    }

    {
        lock_guard<mutex> avnGuard(avnListMutex);
        avnList.push_back(avn);
    }

    lock_guard<mutex> guard(coutMutex);
    cout << "AVN GENERATED: ID=" << avn.avnID
         << ", Flight=" << avn.flightNumber
         << ", Airline=" << avn.airlineName
         << ", Type=" << getAircraftTypeName(avn.aircraftType)
         << ", Phase=" << getPhaseName(aircraft.phase);

    if (speedViolation) 
        cout << ", Speed=" << avn.speedRecorded << " km/h (Permissible: " << avn.speedPermissible << ")";
    if (altitudeViolation) 
        cout << ", Altitude=" << avn.altitudeRecorded << " ft (Permissible: " << avn.altitudePermissible << ")";
    
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&avn.issueTime));
    cout << ", Issued=" << timeStr;
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&avn.dueDate));
    cout << ", Due=" << timeStr
         << ", Fine=PKR " << fixed << setprecision(2) << avn.fineAmount
         << ", Status=" << avn.paymentStatus << endl;
    cout << "AVN forwarded to Airline Portal and StripePay.\n";
}

void updateAVNPaymentStatus(const string& avnID) 
{
    lock_guard<mutex> guard(coutMutex);
    for (auto& avn : avnList) 
    {
        if (avn.avnID == avnID) 
        {
            avn.paymentStatus = "paid";

            // set aircraft's hasAVN to false
            auto it = aircraftStatusMap.find(avn.flightNumber);
            if (it != aircraftStatusMap.end()) 
            {
                Aircraft* aircraft = it->second;
                aircraft->hasAVN = false;
            }

            // Notifying ATC Controller and Airline Portal
            cout << "PAYMENT CONFIRMED: AVN ID=" << avnID
                 << ", Flight=" << avn.flightNumber
                 << ", Airline=" << avn.airlineName
                 << ", Fine=PKR " << fixed << setprecision(2) << avn.fineAmount
                 << ", Status=paid\n";
            cout << "Notified ATC Controller and Airline Portal.\n";
            return;
        }
    }    
    cout << "ERROR: AVN ID=" << avnID << " not found for payment update.\n";
}

void checkForViolations(Aircraft& aircraft) 
{
    bool speedViolation = false;
    bool altitudeViolation = false;

    // THE RUTHLESS CHECKS:
    switch (aircraft.phase) 
    {
        case HOLDING:
            speedViolation = (aircraft.speed > 600);
            altitudeViolation = (aircraft.altitude < 9000 || aircraft.altitude > 20000);
            break;
        case APPROACH:
            speedViolation = (aircraft.speed < 240 || aircraft.speed > 290);
            altitudeViolation = (aircraft.altitude < 3000 || aircraft.altitude > 10250);
            break;
        case LANDING:
            speedViolation = (aircraft.speed > 240);
            altitudeViolation = (aircraft.altitude > 3100);
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
            altitudeViolation = (aircraft.altitude < 900 || aircraft.altitude > 20000);
            break;
        case ACCELERATING_TO_CRUISE:
            speedViolation = false;
            altitudeViolation = false;
            break;
        case DEPARTURE_CRUISE:
            speedViolation = (aircraft.speed < 800 || aircraft.speed > 900);
            altitudeViolation = (aircraft.altitude < 29000 || aircraft.altitude > 40000);
            break;
        default:
            break;
    }

    if ((speedViolation || altitudeViolation) && !aircraft.hasAVN) 
    {
        aircraft.hasAVN = true;
        aircraft.airline->violations++;
        generateAVN(aircraft, speedViolation, altitudeViolation);
    }
}

void handleGroundFaults(vector<Aircraft*>& aircrafts) 
{
    for (auto& aircraft : aircrafts) 
    {
        if ((aircraft->phase == TAXI || aircraft->phase == AT_GATE) && !aircraft->isFaulty) 
        {
            if (rand() % 100 < 5) 
            {
                aircraft->isFaulty = true;

                lock_guard<mutex> guard(coutMutex);
                cout << "FAULT DETECTED: " << aircraft->flightNumber << " ("
                     << aircraft->airline->name << ") has a ground fault and is being towed." << endl;
            }
        }
    }
}

void displayStatus(const vector<Aircraft*>& aircrafts, const vector<Runway>& runways, int elapsedTime) 
{
    lock_guard<mutex> guard(coutMutex);

    int minutes = elapsedTime / 60;
    int seconds = elapsedTime % 60;

    cout << "\n\n=== AirControlX Status at " << setw(2) << setfill('0') << minutes
         << ":" << setw(2) << setfill('0') << seconds << " ===" << endl;

    cout << "\nRunway Status:" << endl;
    for (const auto& runway : runways) 
    {
        cout << runway.name << ": ";
        if (runway.isOccupied && runway.currentAircraft != nullptr) 
        {
            cout << "Occupied by " << runway.currentAircraft->flightNumber
                 << " (" << runway.currentAircraft->airline->name << ")";
        }
        else
            cout << "Available";

        if (!runway.waitingQueue.empty())
            cout << " | Waiting: " << runway.waitingQueue.size() << " aircraft";        
        cout << endl;
    }

    cout << "\nArrival Queue: " << arrivalQueue.size() << " aircraft" << endl;
    cout << "Departure Queue: " << departureQueue.size() << " aircraft" << endl;

    cout << "\nActive Aircraft (" << aircrafts.size() << "):" << endl;
    for (const auto& aircraft : aircrafts) 
    {
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

        switch (aircraft->priority) 
        {
            case EMERGENCY_PRIORITY: cout << "Emergency"; break;
            case HIGH_PRIORITY: cout << "High"; break;
            case NORMAL_PRIORITY: cout << "Normal"; break;
        }

        // Display estimated wait time for all aircraft
        cout << ", Est. Wait: " << aircraft->estimatedWaitTime << "s";

        // Display actual wait time for aircraft in waiting queue
        if (aircraft->status == "Waiting for Runway")
            cout << ", Actual Wait: " << aircraft->actualWaitTime << "s";
        
        cout << (aircraft->hasAVN ? " [AVN ISSUED]" : "") << endl;
    }

    cout << setfill(' ') << "==================================" << endl << flush;
}

void displayAVNStatistics(const vector<Airline>& airlines) 
{
    lock_guard<mutex> guard(coutMutex);
    
    // Column widths
    const int nameWidth = 25;
    const int typeWidth = 12;
    const int violationsWidth = 12;

    // Calculate total violations
    int totalViolations = 0;
    double totalFines = 0.0;
    int unpaidCount = 0;
    for (const auto& airline : airlines) {
        totalViolations += airline.violations;
    }
    for (const auto& avn : avnList) {
        totalFines += avn.fineAmount;
        if (avn.paymentStatus == "unpaid") {
            unpaidCount++;
        }
    }
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
    for (const auto& airline : airlines) 
    {
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
    cout << "| Total Fines: PKR " << fixed << setprecision(2) << totalFines
         << setw(nameWidth + typeWidth - 14) << " |" << setw(violationsWidth) << " " << " |\n";
    cout << "| Unpaid AVNs: " << unpaidCount
         << setw(nameWidth + typeWidth - 12) << " |" << setw(violationsWidth) << " " << " |\n";
    cout << "=============================\n";
    



    if (!avnList.empty()) 
    {
        cout << "\nDetailed AVN List:\n";
        for (const auto& avn : avnList) 
        {
            cout << "AVN ID: " << avn.avnID
                 << ", Flight: " << avn.flightNumber
                 << ", Airline: " << avn.airlineName
                 << ", Type: " << getAircraftTypeName(avn.aircraftType)
                 << ", Fine: PKR " << fixed << setprecision(2) << avn.fineAmount
                 << ", Status: " << avn.paymentStatus;
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&avn.issueTime));
            cout << ", Issued: " << timeStr;
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&avn.dueDate));
            cout << ", Due: " << timeStr << endl;
        }
    }
    cout << "\n" << flush;
}

string getPhaseName(FlightPhase phase) 
{
    switch (phase) 
    {
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

string getDirectionName(Direction dir) 
{
    switch (dir) 
    {
        case NORTH: return "North";
        case SOUTH: return "South";
        case EAST: return "East";
        case WEST: return "West";
        default: return "Unknown";
    }
}

string getAircraftTypeName(AircraftType type) 
{
    switch (type) 
    {
        case COMMERCIAL: return "Commercial";
        case CARGO: return "Cargo";
        case EMERGENCY: return "Emergency";
        default: return "Unknown";
    }
}


void removeAircraftFromEverywhere(Aircraft* aircraft, vector<Aircraft*>& aircrafts, vector<Runway>& runways) 
{
    auto it = find(aircrafts.begin(), aircrafts.end(), aircraft);
    
    if (it != aircrafts.end()) 
        aircrafts.erase(it);
    

    for (auto& runway : runways) 
    {
        if (runway.currentAircraft == aircraft) 
        {
            runway.isOccupied = false;
            runway.currentAircraft = nullptr;
        }
        queue<Aircraft*> tempQueue;
        while (!runway.waitingQueue.empty()) 
        {
            Aircraft* queuedAircraft = runway.waitingQueue.front();
            runway.waitingQueue.pop();

            if (queuedAircraft != aircraft)
                tempQueue.push(queuedAircraft);            
        }
        runway.waitingQueue = tempQueue;
    }

    aircraftStatusMap.erase(aircraft->flightNumber);
    priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> tempArrivalQueue;

    while (!arrivalQueue.empty()) 
    {
        Aircraft* queuedAircraft = arrivalQueue.top();
        arrivalQueue.pop();

        if (queuedAircraft != aircraft)
            tempArrivalQueue.push(queuedAircraft);        
    }
    arrivalQueue = tempArrivalQueue;

    priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority> tempDepartureQueue;
    while (!departureQueue.empty()) 
    {
        Aircraft* queuedAircraft = departureQueue.top();
        departureQueue.pop();

        if (queuedAircraft != aircraft)
            tempDepartureQueue.push(queuedAircraft);        
    }
    departureQueue = tempDepartureQueue;

    // Delete the aircraft
    delete aircraft;
}


void rebuildQueue(Aircraft* aircraft, priority_queue<Aircraft*, vector<Aircraft*>, ComparePriority>& queue) 
{
    vector<Aircraft*> temp;
    bool found = false;

    while (!queue.empty()) 
    {
        Aircraft* a = queue.top();
        queue.pop();
        if (a == aircraft) 
            found = true;
        else 
            temp.push_back(a);        
    }

    if (found) 
    {
        for (auto a : temp) 
            queue.push(a);        
        queue.push(aircraft);
    }
}




bool isAircraftWaiting(const Aircraft& aircraft, const vector<Runway>& runways) 
{
    for (const auto& runway : runways) 
    {
        queue<Aircraft*> tempQueue = runway.waitingQueue;
        while (!tempQueue.empty()) 
        {
            Aircraft* queuedAircraft = tempQueue.front();
            tempQueue.pop();

            if (queuedAircraft == &aircraft) 
                return true;            
        }
    }
    return false;
}