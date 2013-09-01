.SUFFIXES: .dts .dtbo

default: BB-MACHXO2-JTAG-00A0.dtbo

install: BB-MACHXO2-JTAG-00A0.dtbo
	cp BB-MACHXO2-JTAG-00A0.dtbo /lib/firmware
	/bin/sh -c "echo BB-MACHXO2-JTAG >/sys/devices/bone_capemgr.7/slots"

test:
	devmem2 0x20000000 b

.dts.dtbo:
	dtc -O dtb -o $@ -b 0 -@ $<
