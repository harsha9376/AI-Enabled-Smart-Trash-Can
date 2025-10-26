# AI-Enabled Smart Trash Can
### Intelligent Waste Classification and Segregation using Deep Learning and IoT

## Overview
Efficient waste management is a critical challenge in urban and residential environments, often hindered by improper segregation at the source.  
This project presents an AI-enabled smart trash can that assists users in real-time waste classification and sorting, combining deep learning and IoT technologies.

The system integrates a camera module and multiple sensors (ultrasonic, IR, and weight sensor) to detect, analyze, and categorize waste as it is deposited.  
A lightweight Convolutional Neural Network (CNN) deployed on an embedded device (e.g. ESP32-CAM) identifies the waste type and directs it to the appropriate bin compartment.

## System Architecture
### **Hardware Components**
- **ESP32-CAM / Raspberry Pi**
- **Ultrasonic sensor (HC-SR04)** – detects object proximity  
- **IR sensor** – detects waste placement  
- **HX711 Load cell module** – measures waste weight  
- **Servo motors** – control bin lid or sorting mechanism  
- **LED / LCD display** – shows classification result  
- **Power supply (5V)**  

### **Software Components**
- **Embedded code (C++ / Arduino IDE)** for ESP32-CAM  
- **Python (TensorFlow / OpenCV)** for CNN model  
- **ThingSpeak** / **Firebase** – cloud IoT data logging  
- **Flask / Node-RED dashboard** – for analytics visualization  

## Features
- Real-time waste detection and classification
- Camera live stream and image capture for verification
- Servo-controlled bin sorting (recyclable / non-recyclable / compost)
- Cloud data upload for waste statistics and trend analysis
- Low-cost and scalable** design suitable for smart cities

### Hardware Setup
1. Assemble the ESP32-CAM module with IR, ultrasonic, and load cell sensors.
2. Connect servo motors to designated GPIO pins.
3. Power the system using 5V adapter or USB supply.

### Software Setup
1. Arduino IDE
2. TensorFlow Lite
