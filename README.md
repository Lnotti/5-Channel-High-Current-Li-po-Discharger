# 5 Channel High Current LiPo Storage Discharger

A PCB designed to discharge 6S LiPo battery packs to storage voltage (3.8V per cell / 22.8V total) across up to 5 independent channels simultaneously.

## Overview

Leaving LiPo packs fully charged for extended periods degrades cell chemistry over time. This board solves that by providing a dedicated, controlled way to bring packs down to storage voltage after a flying session without needing a full featured charger to do it.

Each of the 5 channels accepts a 6S pack via an XT60 connector and independently discharges it to 22.8V, cutting off automatically when the target is reached. Channels are fully isolated from each other so packs at different voltages can be discharged simultaneously without risk.

## Status

The first physical prototype has been sent off for manufacture. This repository will be updated as boards arrive and testing begins. Additional revisions are planned based on real world testing results.

## Hardware

- STM32F103C8T6 microcontroller
- PCA9685 16 channel PWM driver for RGB status LEDs
- Active MOSFET load per channel with wirewound power resistors
- 5V buck converter and 3.3V LDO onboard power supply
- USB-C port for firmware flashing via DFU bootloader
- Active cooling via 40mm 5V fan
- Per channel RGB status LED indicating discharge state
- SWD header for live debugging

## Specifications

- Input: 6S LiPo (22.2V - 25.2V)
- Channels: 5 independent
- Discharge current: approximately 1A per channel
- Target storage voltage: 22.8V (3.8V per cell)
- Connector: XT60 female PCB mount
- Board dimensions: 160 x 110mm

## Planned Revisions

- Case design with integrated fan mounting and ventilation
- Thermal testing and validation
- Firmware development and testing
- Potential support for additional cell counts
- Further refinement based on prototype testing

## Repository Contents

- KiCad schematic and PCB layout files
- Gerber files for manufacture
- Bill of materials
