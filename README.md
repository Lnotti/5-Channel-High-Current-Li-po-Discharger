# 5 Channel High Current LiPo Storage Discharger

A PCB designed to discharge 6S LiHV battery packs to storage voltage (3.8V per cell / 22.8V total) across up to 5 independent channels simultaneously.

## Overview

Leaving LiPo packs fully charged for extended periods degrades cell chemistry over time. This board solves that by providing a dedicated, controlled way to bring packs down to storage voltage after a flying session without needing a full featured charger to do it.

Each of the 5 channels accepts a 6S pack via an XT60 connector and independently discharges it to 22.8V, cutting off automatically when the target is reached. Channels are fully isolated from each other so packs at different voltages can be discharged simultaneously without risk.

## Problems with current solutions
I have a lipo charger that doubles as a lipo discharger, but it falls short in many aspects.
- Cost ($100+)
- Only two channels
- Discharge current only 0.2A max
- Slows down immensely close to the end of discharge cycle
I also have small lipo discharger that clip onto individual packs that also fall short
- Takes 3+ hours to discharge to storage
- Only has a light to let me know that it has finished (I forget about them since they are so small)
- High leakage current
- High leakage current combined with the time taken to discharge combined with their forgettability leads to frequent overdischarging making me plug them back in to charge ruining the ease

## Status

Version 0.1.1 is a complete board redesign addressing multiple hardware issues identified during review of the 0.1.0 prototype. The revised board has been submitted for manufacture. Testing and firmware development are ongoing.

## Hardware

- STM32F103C8T6 microcontroller
- Active MOSFET load per channel with wirewound power resistors
- Buck converter and 3.3V LDO onboard power supply
- USB-C port for firmware flashing via DFU bootloader
- Active cooling via 40mm 5V fan
- Power status LED for supply rail debugging
- SWD header for live debugging
- Multiple STM32 connectivity options: USB, I2C, ST-Link, UART, DFU

## Specifications

- Input: 6S LiHV (22.8V - 26.1V)
- Channels: 5 independent
- Discharge current: approximately 1A per channel
- Target storage voltage: 22.8V (3.8V per cell)
- Connector: XT60 female PCB mount
- Board dimensions: 160 x 110mm

## Version History

### v0.1.1 — Complete Board Redesign
*Current version*

A full redesign addressing hardware errors found during schematic and layout review of v0.1.0.

**Bug fixes:**
- Corrected footprints for large wirewound power resistors which were wrong in v0.1.0
- Fixed STM32F103C8T6 footprint which was incorrect in v0.1.0
- Replaced buck converter with a higher rated part after identifying that the original selection would be insufficient for LiHV pack voltages (26.1V max vs 25.2V assumed for standard LiPo)
- Updated voltage divider resistor values (1M / 120k) to bring the ADC sense voltage within safe range for LiHV full charge voltage — previous values would have exceeded the STM32 3.3V ADC input limit at 26.1V
- Updated voltage dividers to higher values to reduce current draw, as to not discharge batteries less than their storage voltage after discharging process is complete.

**Design changes:**
- Discharge resistors repositioned to the center of the board based on thermal simulation results for improved heat distribution
- Removed per-channel RGB LEDs to simplify bring-up and reduce scope; power system debugging is now the immediate priority
- Added power rail status LED to aid debugging of the onboard supply
- Added multiple STM32 connectivity options: USB, I2C, ST-Link, UART, DFU headers
- Removed oversized battery net traces; 1A continuous current does not require large copper pours
- Redesigned discharge channels using hierarchical sheets for layout uniformity across all 5 channels and easier future edits
- General board layout cleanup and component alignment

**Known limitations:**
- RGB per-channel status LEDs removed; may be reintroduced in a later revision once core functionality is validated
- Unsure of how precise the mosfet discharge current in relation to gate voltage will be, this may create issues with precision at the end of a discharge cycle limiting speed.
- Firmware development not yet started
- Unsure of what control loop I will use to handle getting to storage voltage at a consistent current with no overshoot

---

### v0.1.0 — Initial Prototype
*First PCB submission. Multiple hardware errors identified prior to and during bring-up.*

- Initial 5-channel discharge board design
- STM32F103C8T6 microcontroller
- PCA9685 16 channel PWM driver for RGB status LEDs
- Per channel RGB status LED
- USB-C DFU bootloader
- SWD debug header
- 160 x 110mm board

**Issues identified:**
- Wrong footprints for wirewound power resistors
- Wrong STM32 footprint
- Buck converter underrated for LiHV input voltage
- Voltage divider ratio unsafe for LiHV full charge voltage on STM32 ADC
- Board layout not uniform across discharge channels

---

## Planned Revisions

- Case design with integrated fan mounting and ventilation
- Thermal testing and validation on physical hardware
- Firmware development and testing
- Potential reintroduction of per-channel RGB status LEDs
- Potential support for additional cell counts
- Further refinement based on prototype testing results

## Repository Contents

- KiCad schematic and PCB layout files
- Gerber files for manufacture
- Bill of materials
