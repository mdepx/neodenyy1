# Firmware for Neoden YY1 Pick And Place machine

### Build under Linux/FreeBSD
    $ export CROSS_COMPILE=arm-none-eabi-
    $ git clone --recursive https://github.com/mdepx/neodenyy1
    $ cd neodenyy1
    $ make

### Read and backup your current firmware
    $ sudo openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -s /home/br/dev/openocd-rpi/tcl -c "adapter speed 5000" -c 'cmsis_dap_vid_pid 0x2e8a 0x000c' -c init -c "reset halt" -c 'flash read_bank 0 firmware_YY1.bin 0 0x80000' -c "reset" -c shutdown

### Program firmware
    $ sudo openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -s /home/br/dev/openocd-rpi/tcl -c "adapter speed 5000;" -c 'cmsis_dap_vid_pid 0x2e8a 0x000c' -c init -c "reset halt" -c 'program obj/neodenyy1.bin reset 0x08000000 exit'

![alt text](https://raw.githubusercontent.com/mdepx/neodenyy1/master/images/neodenyy1.jpg)
