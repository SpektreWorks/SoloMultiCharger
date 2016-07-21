# SoloMultiCharger
A fully integrated smart circuit to charge up to four Solo Smart Batteries, a Solo hand controller, and two USB devices.

INTRODUCTION

The SpektreWorks Solo Multi-Charger is a multi-battery charging station to be used with the 3DR Solo. It has charging ports for four 3DR Solo Smart Batteries, a Solo hand controller, and two USB devices. SpektreWorks has released all of the design files as open source to the DIY community to build and modify the Multi-Charger. 

The project is made up of a schematic, PCB layout, bill of materials (BOM), and firmware. It is offered as a DIY kit with instructions to make the board, populate it with components, and program the necessary firmware. The schematic and PCB layout are available in Cadsoft Eagle format. The firmware is written in C and compiled with Microchip's XC8 compiler using MPLAB X IDE.
  


DETAILS

The SpektreWorks Solo Multi-Charger is a single circuit 4-layer PCB that can charge up to four Solo Smart Batteries, the hand controller, and two USB devices. The board requires an external 24VDC power supply capable of at least 5A.

In order to keep the component count and power requirements to a reasonable level, only two batteries charge at a time. Once a battery is done charging, the circuit automatically begins charging the next battery in the queue with no user intervention. The circuit always charges the battery with highest state of charge first. This way, the user only has to wait the shortest possible time to retrieve a fully charged battery.

The Smart Battery and hand controller charge ports are actively current limited. This means that the circuit adjusts the voltage on these ports to not exceed a maximum current value. For the Smart Batteries, the maximum current is 3.3A. The hand controller is 1.5A. These values are adjustable in the firmware, however, it is not recommended to increase those numbers.

The design files in this kit do not include an enclosure of any kind. Users are encouraged to design their own enclosure for the purpose. Keep in mind, however, that this board can generate a lot of heat. Proper heat-sinking and air flow considerations are a very important part of the design.
  


RELEVANT FILES

PCB/Solo_MultiCharger.sch - Cadsoft Eagle Schematic  
PCB/Solo_MultiCharger.brd - Cadsoft Eagle PCB layout  
PCB/Solo_MultiCharger_BOM.csv - Bill of Materials  
Firmware/Solo_MultiCharger.X/main.c - Source code for charging circuit  
Firmware/Solo_MultiCharger.X/Makefile - Required Makefile for MPLAB  
Firmware/Solo_MultiCharger.X/nbproject - Required directory for MPLAB  
front.jpg - photo of fully assembled front of PCB  
back.jpg - photo of fully assembled back of PCB  
  


INSTRUCTIONS

**Note: These are extremely abbreviated instructions. We hope that users will suggest what sections need to be elaborated and what sections are fine as they are. As time permits, we will try to expand the instructions until even a relatively inexperienced hobbyist can create their own boards on their own.**

Prerequisites:  
	-Basic familiarity with fabricating PCBs, ordering parts, and soldering  
	-MPLAB X software (free from Microchip) with XC8 compiler (also free)  
	-PICKit3 programming pod  

1. Manufacture the PCB. We did our prototypes at OSHPark.com. We also highly recommend ordering a stencil. This design has a few fine-pitched components that are a little difficult (but not impossible) to solder by hand. Cheap stencils can be ordered from OSHStencils.com.

2. Order the BOM. Every component should be available at digikey.com. If a component is no longer stocked, feel free to contact us and we'll help you find a replacement part.

3. Solder the components to the board.

4. Check the board for shorts. Connect an adjustable current limited power supply and test at 24V. There should only be a few mA of current draw from the supply. If there's more than just a few mA, turn off power and check for solder errors.

5. Check each of the voltages for each switching regulator on the board. For the hand controller and USB outputs, a voltmeter can be applied directly to the charging port pads. The hand controller should show 8.3V and USB should show 5V. The smart battery ports, however, are switched with p-Channel MOSFETs, so they won't always show the required 16.8V. It will be necessary to look at the schematic and probe the capacitors downstream of the inductors of both switching regulators. You should see a steady 16.8V.

5. Open the firmware project in MPLAB X and compile. Confirm there are no compile errors (a couple of “warning (752)” are ok).

6. Connect the board to your computer using a PICKit 3 or equivalent PIC programmer. Program the firmware into the board.

At this point you should be able to connect your 24VDC power supply and start charging batteries, the hand controller, and USB devices. Obviously don't just plug everything in until you've individually tested each port.

Pro Tips:  
	-This board will get hot! Do not place it in an unventilated enclosure. Forced air cooling is always best!  
	-Keep your wires as short as possible. Very long wires can cause large voltage transients when current is suddenly started or stopped.  
	-The four mounting holes are not grounded. If you wish to ground your board to a metal enclosure, you can tap off any solder pad with a negative sign (“-”) next to it.
