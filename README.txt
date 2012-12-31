bonemachxo
==========

BeagleBone + MachXO breakout board

Fooling around with a BeagleBone (beagleboard.org) connected to a Lattice
MachXO breakout board (http://www.latticesemi.com/products/developmenthardware/developmentkits/machxo2breakoutboard.cfm)

This is just for the fun of it, so no promises how far this will go.

1. Programming the MachXO 1200ZE chip using SPI and/or I2C.
2. Emulating cape eeprom using MachXO hard I2C.
3. Interfacing to the MachXO on GPMC.
4. Making a reasonable efficient JTAG master in the FPGA.
5. Making a driver for this JTAG master in OpenOCD.
6. Extend the JTAG master to be able to run SWD.  Make driver for this also.
7. Interface to ARM trace signals and software to read the trace data.
8. Design a cape using a MachXO FPGA as a Ethernet-enabled debug probe.

Contributions are of course welcome.

License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
