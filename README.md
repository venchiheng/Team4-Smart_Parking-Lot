# 🚗 Smart Parking Gate System using ESP32

This project implements an intelligent parking gate system using the ESP32 microcontroller. It automates vehicle entry and exit, monitors parking availability, and uploads data to the cloud via ThingSpeak.

## 🔧 Features

- Detects vehicle entry and exit using IR sensors
- Controls servo motor to open and close the gate
- Tracks real-time vehicle count and daily entries
- Updates ThingSpeak cloud dashboard
- Displays status with LED indicators:
  - 🟢 Green: Available
  - 🔴 Red: Full
  - 🟡 Yellow: Gate opening/closing

---

## 📦 Components Used

| Component            | Description                               |
|----------------------|-------------------------------------------|
| ESP32 Dev Board      | Main controller with Wi-Fi capabilities   |
| IR Sensor Module (x2)| Entry and Exit vehicle detection          |
| SG90 Servo Motor     | Controls parking gate movement            |
| LEDs (R/G/Y)         | Status indicators                         |
| Jumper Wires + Breadboard | For connections and circuit setup   |

---