# ESP32 Biosignal Monitor


![ECG](https://github.com/user-attachments/assets/98ec4467-1ff1-4b30-bbcb-ce5a2efe765d)


A portable biosignal monitoring system using ESP32 that captures ECG (AD8232) and PPG (HW827) data and streams it to Firebase Realtime Database.

## Features

- Real-time ECG and PPG signal acquisition
- Signal processing with moving average and IIR filters
- WiFi connectivity with auto-reconnect
- Firebase Realtime Database integration
- Memory-efficient buffering system
- Configurable sampling rate (default: 100Hz)

## Hardware Requirements

- ESP32 development board
- AD8232 ECG sensor
- HW827 Pulse/PPG sensor
- Jumper wires and electrodes

## Setup Instructions

1. Clone this repository
2. Fill in your credentials 
3. Connect hardware as shown in the wiring diagram
4. Upload to ESP32 using PlatformIO or Arduino IDE

### Wiring Diagram

| ESP32 Pin | AD8232 Pin | HW827 Pin |
|-----------|------------|-----------|
| 3.3V      | 3.3V       | VCC       |
| GND       | GND        | GND       |
| 36 (VP)   | OUTPUT     | -         |
| 35        | -          | OUT       |

## Configuration

Set Your :
- WiFi credentials
- Firebase settings
- Sampling parameters

## Firebase Data Structure

Data is stored in the following format:
```json
{
  "sensor_data": {
    "timestamp": {
      "sample_rate": 100,
      "dataPoints": [
        {
          "ppg": 123.45,
          "ecg": 678.90,
          "timestamp": 1234567890
        }
      ]
    }
  }
}
