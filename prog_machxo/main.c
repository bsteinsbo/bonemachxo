/*
 * Program for programming Lattice MachXO2 FPGA's.
 * Copyright (c) 2013 Bjarne Steinsbo <bjarne at steinsbo dot com>
 * License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "machxo.h"
#include "jedec.h"

static void initialize_flash()
{
	if (check_device_id_quick() != 1)
	{
		fprintf(stderr, "Device ID doesn't make sense.  Exiting.\n");
		exit(1);
	}
	if (enable_offline_configuration() != 1 || wait_not_busy() != 1)
	{
		fprintf(stderr, "Failed to enable configuration.\n");
		exit(1);
	}
	if (erase_flash() != 1 || wait_not_busy() != 1)
	{
		fprintf(stderr, "Failed to erase flash.\n");
		exit(1);
	}
}

static int all_zero(uint8_t *data, int data_len)
{
	int i;
	for (i = 0; i < data_len; i++)
		if (data[i] != 0)
			return 0;
	return 1;
}

static void abort_and_clean_up(char *message)
{
	erase_flash();
	wait_not_busy();
	refresh();
	if (message != 0)
		fprintf(stderr, "%s\n", message);
	fprintf(stderr, "Aborting. Flash is erased.\n");
	exit(1);
}

static void do_work()
{
	int section;
	uint32_t address;
	uint8_t *data;
	int data_len;
	int tag_data_seen = 0;
	int page_address;
	int i;

	// Assume there will be one initial section that we can ignore
	if (get_next_jedec_section(&section, &address, &data, &data_len) != 1)
		return;
	initialize_flash();
	while (1)
	{
		if (get_next_jedec_section(&section, &address, &data, &data_len) != 1)
		{
			abort_and_clean_up("Input file error.");
			return;
		}
		switch (section)
		{
		case SECTION_NOTE:
			if (strstr((char*)data, "TAG DATA") != 0)
				tag_data_seen = 1;
			break;
		case SECTION_FUSE_MAP:
			if ((address / MACHXO2_PAGE_SIZE) * MACHXO2_PAGE_SIZE != address)
				abort_and_clean_up("Flash address not multiple of page size");
			if ((data_len / MACHXO2_PAGE_SIZE) * MACHXO2_PAGE_SIZE != data_len)
				abort_and_clean_up("Data block size not multiple of page size");
			if (!all_zero(data, data_len))
			{
				page_address = address / MACHXO2_PAGE_SIZE;
				if (set_configuration_flash_address(page_address, tag_data_seen) != 1)
					abort_and_clean_up("Failed to set flash address");
				for (i = 0; i < data_len; i += MACHXO2_PAGE_SIZE)
				{
					if (program_configuration_flash(&data[i], MACHXO2_PAGE_SIZE) != 1 || wait_not_busy() != 1)
						abort_and_clean_up("Failed to program device.");
				}
			}
			break;
		case SECTION_ARCH:
			if (data_len != 10)
				abort_and_clean_up("Incorrect size feature row and bits");
			if (program_feature_row(data) != 1 || wait_not_busy() != 1)
				abort_and_clean_up("Failed to program feature row");
			if (program_feature_bits(data + 8) != 1 || wait_not_busy() != 1)
				abort_and_clean_up("Failed to program feature bits");
			break;
		case SECTION_USERCODE:
			if (program_user_code(address) != 1 || wait_not_busy() != 1)
				abort_and_clean_up("Failed to program user code");
			break;
		case SECTION_END:
			program_done() != 1 || wait_not_busy() != 1 || refresh() != 1 || wait_not_busy() != 1;
			return;
		case SECTION_NUM_PINS:
		case SECTION_NUM_FUSES:
		case SECTION_DEFAULT_FUSE_VAL:
		case SECTION_CHECK_SUM:
			break; // just ignore for now
		case SECTION_SECURITY_FUSE:
			if (data[0] != '0')
				fprintf(stderr, "Security fuse not implemented");
			break;
		default:
			abort_and_clean_up("Unknown JEDEC section");
		}
	}
}

static void print_usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [-d <device>] [-a <i2c_addr>] <jedec file>\n", prog);
	fputs("  -d   device to use (default /dev/spidev2.0)\n"
	      "  -a   i2c address\n", stderr);
	exit(1);
}

int main(int argc, char **argv)
{
	char *device_file = DEFAULT_SPI_DEV;
	int mode = MODE_SPI;
	int i2c_addr = 0x40;
	char *prog_name = "prog_machxo";
	if (argc < 2)
		print_usage(prog_name);
	argc--; argv++;
	while (argv[0][0] == '-')
	{
		if (argc < 3)
			print_usage(prog_name);
		if (argv[0][1] == 'd')
			device_file = argv[1];
		else if (argv[0][1] == 'a')
		{
			i2c_addr = atoi(argv[1]);
			mode = MODE_I2C;
		}
		else
			print_usage(prog_name);
		argv += 2;
		argc -= 2;
	}
	if (open_jedec(argv[0]) != 1)
		return 1;
	if (open_device(0, mode, i2c_addr) != 1)
		return 1;
	do_work();
  //initialize_flash();
	return 0;
}