# Firmware for Neoden YY1 Pick-And-Place machine

This is a conversion kit for your YY1 to make it OpenPnP-compatible.

Project status: working, managed to assemble a few boards with no issues.

| part              | status  |  notes |
| ----------------- | ------- | ------ |
| Motion controller | functional              | gcode parts needed for speed control and homing     |
| Vision            | functional              | openpnp's vision pipeline needs tuning |
| LED ring bottom   | prototyping in progress | |
| LED ring top      | prototyping in progress | |

Note that the information below is not complete, I am keep updating it.

### Hardware overview.

YY1 features several electronics parts: main board (motion controller), 3x displays and camera boards. All parts are connected to each other using either UART or SPI, there is no USB in entire system.

Main board is build around STM32F407VET6 micro-controller and LV8729 steppers.

The vision is done on the edge, i.e. directly on the PCBs the camera sensors are located on. Each features STM32H7. The camera sensor handled by DCMI (Digital Camera Interface) stm32's peripheral device. Firmware on both cameras are identical. The result of machine vision is then passed over low speed serial interface to the main board. The stm32 pins exposed to connectors on the camera modules could not be remapped to USB.

Each of camera module is connected to its display for HMI over low speed as well.

Main display is connected to the main board over UART.
We will use this UART for both communication to OpenPnP over GCode (input/output) and developer console to the stm32f407 (output only).

![Stepper board](https://raw.githubusercontent.com/mdepx/neodenyy1/master/images/stepper_board.jpg)

### What you need to do

 1) Replace both cameras (for OpenPnP we need USB cameras)
 2) Update firmware
 3) Setup OpenPnP

### What this project gives to you

 1) New firmware for motion controller written from scratch. The firmware uses real-time operating system [MDEPX](https://github.com/mdepx/mdepx) that I wrote from scratch as well.
 2) CAD models so you can assemble your own USB camera modules and LED rings (TODO)
 3) No warranty of any kind. Do that on your own risk!

### Debug tools

Locate SWD pins (Data / Clock / Ground) on the stepper board and solder down 2.54mm header.

Any standard Cortex SWD debugger should work with those pins. Instructions below are for [Debug Probe](https://www.raspberrypi.com/products/debug-probe/).

### Download and install OpenOCD

I use [OpenOCD fork from RPI](https://github.com/raspberrypi/openocd.git).
(Clone and build using instructions provided at the link)

### Build this firmware under Linux/FreeBSD
    $ export CROSS_COMPILE=arm-none-eabi-
    $ git clone --recursive https://github.com/mdepx/neodenyy1
    $ cd neodenyy1
    $ make

Note there is no support for build under Windows.

### Read and backup your current firmware

Replace tcl path to your openocd installation directory

    $ sudo openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
        -s /home/br/dev/openocd-rpi/tcl -c "adapter speed 5000" \
        -c 'cmsis_dap_vid_pid 0x2e8a 0x000c' -c init -c "reset halt" \
        -c 'flash read_bank 0 firmware_YY1.bin 0 0x80000' \
        -c "reset" -c shutdown

### Program firmware
    $ sudo openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
        -s /home/br/dev/openocd-rpi/tcl -c "adapter speed 5000" \
        -c 'cmsis_dap_vid_pid 0x2e8a 0x000c' -c init -c "reset halt" \
        -c 'program obj/neodenyy1.bin reset 0x08000000 exit'

### Operation

Note that by default the firmware will home the machine on startup. Homing button in the OpenPnP is not implemented (yet).
Note that this firmware converts linear motion of Z coordinate into rotational. When you setup Z axis in the OpenPnP use ReferenceControllerAxis (linear motion).

### Camera modules

You need these parts

    - M2 standoff round spacers https://www.aliexpress.com/item/4001270070683.html
    - M3 standoff hex spacers https://www.aliexpress.com/item/4001242054845.html
    - Wera 118124 Kraftform Micro Nut Spinner 5mm (for M3 standoffs)
    - Sony IMX415 Top and Bottom cameras https://www.aliexpress.com/item/1005005481109987.html
    - Some M12 lens on your choice (I'm still experimenting with different)

### Contribution

Any improvements are welcome! Note that your patches and pull-requests (if any) use FreeBSD's [style(9) guide](https://man.freebsd.org/cgi/man.cgi?style(9)) and BSD 2-clause license.

### LQFP128 vision result

![LQFP](https://raw.githubusercontent.com/mdepx/neodenyy1/master/images/lqfp_vision.gif)

### Help

If you are seeking for help for this project or need firmware for another PnP / other types of equipment, then you can reach me using br@bsdpad.com.
Also my telegram channel [@machinedependent](https://t.me/machinedependent) and the chat linked to it are available.

![NeoDen YY1](https://raw.githubusercontent.com/mdepx/neodenyy1/master/images/neodenyy1.jpg)
