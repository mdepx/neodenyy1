# neodenyy1

### Build under Linux/FreeBSD

    $ export CROSS_COMPILE=arm-none-eabi-
    $ git clone --recursive https://github.com/machdep/neodenyy1
    $ cd neodenyy1
    $ make

### Program mdepx
    $ sudo openocd -s /path/to/openocd/tcl -f interface/stlink-v2-1.cfg -f target/stm32f4x.cfg -c 'program obj/neodenyy1.bin reset 0x08000000 exit'

![alt text](https://raw.githubusercontent.com/machdep/neodenyy1/master/images/neodenyy1.jpg)
