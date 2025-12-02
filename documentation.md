# Water Dispenser Control System Documentation

## Overview
This project implements a control system for a water dispenser using an STM32F103C8T6 ("Blue Pill") microcontroller. It manages the water pump, monitors sensors (door switch, water level), and provides status feedback via LEDs.

## Directory Structure (`Core`)
The core logic is contained within the `Core` directory:
- **`Inc/`**: Header files.
- **`Src/`**: Source files.
- **`Startup/`**: Assembly startup code.

### Key Files
| File | Description |
|------|-------------|
| `config.h` | **Primary Configuration File**. Contains all user-adjustable parameters (timings, polarity, sensor types). |
| `state_machine.c/.h` | Implements the core system logic using a finite state machine. |
| `sensors.c/.h` | Handles sensor readings with debouncing and abstraction. |
| `main.c` | Entry point, hardware initialization, and main loop. |

## System Architecture

### 1. State Machine (`state_machine.c`)
The system operates based on a Finite State Machine (FSM) with the following states:

| State | Description |
|-------|-------------|
| `STATE_IDLE` | System is ready. Waiting for door open or tank empty signal. |
| `STATE_DOOR_OPEN` | Door is open. Pump is forced OFF. Waiting for door to close. |
| `STATE_WAIT_SETTLE` | Waiting period after door closes to allow water level to stabilize. |
| `STATE_FILLING` | Pump is ON. Filling the tank until the sensor triggers or timeout occurs. |
| `STATE_FULL` | Tank is full. Pump is OFF. |
| `STATE_COOLDOWN` | Optional cooldown period after a fill cycle to protect the pump. |
| `STATE_ERROR` | Error condition (e.g., pump timeout, sensor fault). LEDs blink rapidly. |

### 2. Sensor Module (`sensors.c`)
Handles inputs from the physical hardware:
- **Door Switch**: Detects if the dispenser door is open.
- **Water Level Sensor**: Detects if the tank is full.
- **Debouncing**: Implements software debouncing (default 50ms) to prevent false triggers.

### 3. Configuration (`config.h`)
The system is highly configurable. Key settings include:
- **Hardware Polarity**: Independent Active LOW/HIGH support for Program LED, Status LED, Pump, Sensors, and Overflow Sensor.
- **Sensor Types**: Support for Normally Open (NO) or Normally Closed (NC) switches.
- **Optional Features**: Overflow Sensor (Disabled by default).
- **Timings**:
  - `PUMP_NORMAL_FILL_TIME`: Expected fill time (default 6 min).
  - `PUMP_MAX_RUN_TIME`: Safety timeout (default 9 min).
  - `MIN_PUMP_INTERVAL`: Pump protection delay.

## LED Patterns Report
The system uses two LEDs to communicate status:
- **Program LED** (System Heartbeat)
- **Status LED** (Water/Fill Status)

| System State | Program LED | Status LED | Meaning |
|--------------|-------------|------------|---------|
| **IDLE** | **ON** (Solid) | **OFF** | System ready, waiting for trigger. |
| **DOOR OPEN** | **Fast Blink** (250ms) | **OFF** | Door is open. Close door to proceed. |
| **WAIT SETTLE** | **ON** (Solid) | **Slow Blink** (500ms) | Waiting for water to settle after door close. |
| **FILLING** | **ON** (Solid) | **Fast Blink** (250ms) | Pump is active, filling tank. |
| **FULL** | **ON** (Solid) | **ON** (Solid) | Tank is full and ready. |
| **COOLDOWN** | **ON** (Solid) | **Slow Blink** (500ms) | Pump cooling down between cycles. |
| **ERROR** | **Fast Blink** | **Fast Blink** | **Critical Error**. LEDs blink alternatingly. |

### Blink Rates
- **Fast Blink**: Toggles every 250ms (2 Hz cycle). Used for active/warning states.
- **Slow Blink**: Toggles every 500ms (1 Hz cycle). Used for waiting/passive states.
- **Error Blink**: Toggles every 125ms (4 Hz cycle). Used for critical errors.

## Hardware Pinout
Defined in `main.h`:
- **Pump Control**: `GPIOC Pin 15`
- **Program LED**: `GPIOC Pin 13` (System Heartbeat)
- **Status LED**: `GPIOC Pin 14` (Water/Fill Status)
- **Door Switch**: `GPIOA Pin 0`
- **Water Sensor**: `GPIOA Pin 1`

## Safety Features
1. **Pump Timeout**: Prevents the pump from running indefinitely if the sensor fails.
2. **Door Safety**: Pump immediately stops if the door is opened.
3. **Rapid Cycling Protection**: Detects and prevents pump damage from frequent short cycles.
4. **Cooldown**: Enforces a rest period between fill cycles.

## Error Codes
| Code | Meaning |
|------|---------|
| `0` | No Error |
| `1` | **Pump Timeout**: Pump ran longer than `PUMP_MAX_RUN_TIME`. |
| `2` | **Sensor Fault**: Unexpected sensor behavior. |
| `3` | **Rapid Cycling**: Pump is cycling too frequently. |
| `4` | **Gallon Empty**: Pump ran for normal fill time but tank is not full. Indicates source gallon is empty. |
| `5` | **Overflow**: Optional overflow sensor triggered. Pump stopped immediately. |

## Gallon Empty / Change Gallon Workflow
The system automatically detects when the source gallon is empty and guides the user through the replacement process:

1.  **Detection**: If the pump runs for `PUMP_NORMAL_FILL_TIME` (default 6 mins) but the tank does not become full, the system triggers **Error 4 (Gallon Empty)**.
2.  **Indication**: Both LEDs will blink rapidly (Error pattern).
3.  **Recovery (Changing Gallon)**:
    - Open the door to replace the gallon.
    - Keep the door open for at least 3 seconds (`ERROR_RESET_DOOR_TIME`).
    - The error will automatically reset.
    - Close the door.
    - The system will resume normal operation (start filling if the tank is not full).

## How to Modify
1. **Change Timings**: Edit `config.h` and adjust values like `PUMP_NORMAL_FILL_TIME`.
2. **Invert Logic**: If using different hardware (e.g., NC instead of NO sensor), change `WATER_SENSOR_TYPE_NO` to `WATER_SENSOR_TYPE_NC` in `config.h`.
