# Firmware for Neoden YY1 Pick And Place machine

This is a conversion kit for your YY1 to make it OpenPnP-compatible.

Hardware overview

![Stepper board](https://raw.githubusercontent.com/mdepx/neodenyy1/master/images/stepper_board.jpg)

### Debug tools

Locate SWD pins (Data / Clock / Ground) on the stepper board and solder down 2.54mm header.

Any standard Cortex SWD debugger should work with those pins. Instructions located below are for [Debug Probe](https://www.raspberrypi.com/products/debug-probe/).

### Download and install OpenOCD

I use [OpenOCD fork from RPI](https://github.com/raspberrypi/openocd.git).

### Build under Linux/FreeBSD
    $ export CROSS_COMPILE=arm-none-eabi-
    $ git clone --recursive https://github.com/mdepx/neodenyy1
    $ cd neodenyy1
    $ make

### Read and backup your current firmware
    $ sudo openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -s /home/br/dev/openocd-rpi/tcl -c "adapter speed 5000" -c 'cmsis_dap_vid_pid 0x2e8a 0x000c' -c init -c "reset halt" -c 'flash read_bank 0 firmware_YY1.bin 0 0x80000' -c "reset" -c shutdown

### Program firmware
    $ sudo openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -s /home/br/dev/openocd-rpi/tcl -c "adapter speed 5000;" -c 'cmsis_dap_vid_pid 0x2e8a 0x000c' -c init -c "reset halt" -c 'program obj/neodenyy1.bin reset 0x08000000 exit'

![NeoDen YY1](https://raw.githubusercontent.com/mdepx/neodenyy1/master/images/neodenyy1.jpg)
