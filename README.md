# AirControlX - Automated Air Traffic Control System 🛫

A multithreaded air traffic control simulation built in C++ using core Operating Systems concepts. AirControlX is designed to manage aircraft arrivals, departures, runway scheduling, airspace violation detection, airline portals, and real-time visual simulations using SFML.

---

## 📌 Project Overview

**AirControlX** simulates an intelligent Air Traffic Control System (ATCS) at a multi-runway international airport. This system is capable of:

- Managing aircraft traffic across 3 runways
- Enforcing airspace and speed/altitude restrictions
- Detecting violations and issuing AVNs (Airspace Violation Notices)
- Allowing airlines to review and pay fines through a simulated portal
- Displaying a real-time visual simulation using the SFML graphics library

---

## 🛠️ Built With

- **Language:** C++
- **Graphics:** [SFML](https://www.sfml-dev.org/) (Simple and Fast Multimedia Library)
- **Threading & Concurrency:** C++ threads, mutexes, synchronization
- **Platform:** Linux (Ubuntu recommended)
- **IDE:** VS Code (Linux environment)

---

## 🧩 Core Features

### ✈️ Flight Management
- Simulates 6 airlines and multiple aircraft types (commercial, cargo, military, medical)
- Scheduled arrivals/departures with direction-based runway allocation
- Handles emergency priorities, runway conflicts, and queueing

###  OS Concepts Implemented
- Multithreading for aircraft lifecycle and ATC processes
- Mutexes for safe access to shared data (e.g., runway, AVN list)
- Thread synchronization to ensure one-aircraft-per-runway policy
- Simulation time management using timers and sleep functions

###  Violation Detection (AVNs)
- Speed/altitude monitoring in all flight phases (holding, approach, landing, taxi, gate)
- Automatic AVN generation with fines and due dates
- Administered via an Airline Portal and StripePay simulation

### 🧾 Airline Portal + StripePay
- Airline login with AVN lookup and payment system
- Fines calculated based on flight type and violation
- Simulated payment confirmation and real-time status update

###  Visual Simulation
- Aircraft movement animations
- Color-coded runways and aircraft
- Status-based transitions for takeoff, landing, and taxiing

---
### Developed by:
 - Muhammad Kaleem Akhtar (23i-0524)  
 - Muhammad Fatik Bin Imran (23i-0655)  
 - Course: Operating Systems (Spring 2025) – FAST-NUCES
