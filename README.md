# Low-Power Active Cooling System for PV Panels to Improve Conversion Efficiency

## 📌 Project Overview
This repository contains the embedded firmware, IoT architecture, and hardware methodology for a **Low-Power Active Cooling System** designed for Photovoltaic (PV) panels developed as a Final Year Project for the degree of Bachelor of Electrical Engineering at Universiti Sains Malaysia (USM).

The system autonomously mitigates the thermal degradation of solar panels operating in hot equatorial climates (which typically lose 0.3% to 0.5% efficiency for every 1°C rise above 25°C). It utilizes a direct water active cooling method combining front-misted water spraying and rear-mounted closed-loop circulation. 

Crucially, the control architecture is completely self-sufficient and off-grid. The ESP32 logic core is powered by an independent energy-harvesting loop, ensuring that the cooling mechanism does not drain any parasitic power from the primary PV array being tested.

## System Architecture

### 1. Energy Harvest, Storage, and Supply
* **Energy Source:** Dedicated 5V 6W Solar PV Panel.
* **Charge Controller:** TP4056 (Applies CC/CV protocol for safe lithium charging).
* **Storage:** 3200mAh Lithium-ion Battery.
* **Power Regulation:** DC-DC Boost Converter stepping up to a stable 5V USB output for the ESP32.

### 2. Microcontroller & Sensor Network
* **Core Logic:** ESP32 DevKit V1 Microcontroller.
* **Thermal Sensors:** DS18B20 1-Wire Digital Temperature Sensors (Mounted on top, middle, and bottom panel zones).
* **Power Telemetry:** INA219 I²C Current & Voltage Sensor Modules.
* **Environmental Trigger:** HW-072 Light Intensity Sensor Module.
* **Timekeeping:** DS3231 RTC Module (Provides absolute timestamp integrity during Wi-Fi drops).
* **Local UI:** 128x64 I²C OLED Display & LED status indicators.

### 3. Actuation Subsystem
* **Driver:** 5V 2-Way Electromechanical Relay Module (Active-Low).
* **Actuators:** Two 12V R385 DC Diaphragm Water Pumps (Powered via an isolated external 12V supply to prevent inductive back-EMF spikes).

## Automated Control Logic
The system relies on a dual-threshold state machine executing non-blocking timer loops (`millis()`) to conserve energy and optimize cooling:

* **Sleep Mode:** If solar irradiance is < 800 W/m², the system remains dormant to prevent parasitic power waste during low-generation periods.
* **Active Mode:** If solar irradiance is > 800 W/m² and the PV panel's average surface temperature reaches **35°C**, the relay is triggered.
* **Duty Cycle:** To maximize the latent heat of vaporization, the pumps activate for exactly **30 seconds**, followed by a strict **120-second evaporative rest phase**.
* **Manual Override:** Users can preemptively actuate the pumps via a physical push button or remote Telegram commands.

## Tri-Pathway IoT Telemetry
The ESP32 maintains a robust, continuous connection to three distinct cloud platforms:
1. **Google Sheets Database:** Formats 16 distinct data parameters (temperatures, voltages, irradiance) into JSON payloads and POSTs them via a Google Apps Script endpoint every 30 seconds for longitudinal performance analysis.
2. **Telegram Bot API:** Dispatches event-driven push notifications for pump activations, low-battery warnings, and system mode changes. Includes an inline keyboard for remote "Emergency Stops" and manual overrides.
3. **Blynk IoT:** Streams real-time telemetry to a remote graphical dashboard.

## Hardware Pin Mapping (ESP32)

| Component / Module | ESP32 GPIO | Signal Type | Function |
| :--- | :--- | :--- | :--- |
| **INA219, OLED, DS3231** | 21 (SDA), 22 (SCL) | I²C Bus | Shared data and clock lines. |
| **DS18B20 (Panel A)** | 4, 13, 14 | 1-Wire | Uncooled panel temperature probes. |
| **DS18B20 (Panel B)** | 25, 26, 27 | 1-Wire | Cooled panel temperature probes. |
| **Pump 1 Relay** | 5 | Digital Out | Triggers front-misted sprayer (Active-Low). |
| **Pump 2 Relay** | 18 | Digital Out | Triggers rear circulation (Active-Low). |
| **Red LED** | 19 | Digital Out | Sleep Mode indicator. |
| **Green LED** | 32 | Digital Out | Active Mode indicator. |
| **HW-072 Irradiance** | 33 | Analog In | Reads incident solar irradiance. |
| **Push Button** | 34 | Digital In | Manual override (Input Pullup). |

## Software Dependencies
To compile the firmware, install the following libraries via the Arduino IDE Library Manager:
* `WiFi.h` & `WiFiClientSecure.h`
* `Wire.h` & `OneWire.h`
* `DallasTemperature.h`
* `Adafruit_INA219.h`
* `Adafruit_SSD1306.h` & `Adafruit_GFX.h`
* `RTClib.h`
* `HTTPClient.h` & `ArduinoJson.h`
* `BlynkSimpleEsp32.h`
* `UniversalTelegramBot.h`

---
*Developed by Steven Chua Hua Xian, 2026.*
