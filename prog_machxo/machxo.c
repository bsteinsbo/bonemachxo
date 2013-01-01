/*
 * Functions for programming Lattice MachXO2 FPGA's.
 * Copyright (c) 2013 Bjarne Steinsbo <bjarne at steinsbo dot com>
 * License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "machxo.h"

static int dev_fd = -1;
static int mode = MODE_SPI;

static uint8_t spi_mode = 0;
static uint8_t spi_bits = 8;
static uint32_t spi_speed = 5000000; // 5 MHz
static uint16_t spi_delay = 0;

static uint16_t i2c_addr = 0x40;

static struct spi_ioc_transfer spi_xfer[3];
static struct i2c_rdwr_ioctl_data i2c_packets;
static struct i2c_msg i2c_messages[2];

//#define DEBUG(x) (x)
#define DEBUG(x)
#define DEBUG2 0

static int send_receive(uint8_t command, uint32_t operand, int direction, uint8_t *data, int data_len)
{
	uint8_t cmd_buffer[4];
	int status;
	int num_xfers;
	num_xfers = (data == 0) ? 1 : 2;
	if (mode == MODE_SPI) num_xfers++;
	int oplen = 4;
	if (dev_fd == -1)
		return 1; // Debug mode
	memset(spi_xfer, 0 , sizeof spi_xfer);
	memset(&i2c_packets, 0, sizeof i2c_packets);
	memset(i2c_messages, 0, sizeof i2c_messages);
	cmd_buffer[0] = command;
	cmd_buffer[1] = (operand & 0xFF0000) >> 16;
	cmd_buffer[2] = (operand & 0x00FF00) >> 8;
	cmd_buffer[3] = (operand & 0x0000FF);
	switch(command)
	{
	case ISC_ENABLE:
	case ISC_ENABLE_X:
	case ISC_DISABLE:
	case LSC_REFRESH:
		oplen = 3;
		break;
	default:
		oplen = 4;
		break;
	}
	if (data_len < 0 || data_len > 4*1024)
	{
		fprintf(stderr, "Incorrect data length %d\n", data_len);
		return 0;
	}
#if DEBUG2
	fprintf(stderr, "send_receive: %02x %02x %02x", cmd_buffer[0], cmd_buffer[1], cmd_buffer[2]);
	if (oplen == 4)
		fprintf(stderr, " %02x", cmd_buffer[3]);
	if (direction == DIRECTION_SEND)
	{
		int j;
		fprintf(stderr, " ->");
		for (j = 0; j < data_len; j++)
			fprintf(stderr, " %02x", data[j]);
		fprintf(stderr, "\n");
	}
#endif
	if (mode == MODE_SPI)
	{
		spi_xfer[0].tx_buf = (unsigned long)cmd_buffer;
		spi_xfer[0].len = 1;
		spi_xfer[0].delay_usecs = 1;
		spi_xfer[1].tx_buf = (unsigned long)(cmd_buffer + 1);
		spi_xfer[1].len = oplen - 1;
		if (data != 0)
		{
			if (direction == DIRECTION_SEND)
				spi_xfer[2].tx_buf = (unsigned long)data;
			else
				spi_xfer[2].rx_buf = (unsigned long)data;
			spi_xfer[2].len = data_len;
		}
		status = ioctl(dev_fd, SPI_IOC_MESSAGE(num_xfers), spi_xfer);
	}
	else if (mode == MODE_I2C)
	{
		i2c_messages[0].addr = i2c_addr;
		i2c_messages[0].flags = 0;
		i2c_messages[0].buf = cmd_buffer;
		i2c_messages[0].len = command == ISC_ENABLE ? 3 : 4;
		if (data != 0)
		{
			i2c_messages[1].addr = i2c_addr;
			i2c_messages[1].flags = direction == DIRECTION_SEND ? 0 : I2C_M_RD;
			i2c_messages[1].buf = data;
			i2c_messages[1].len = data_len;
		}
		i2c_packets.msgs = i2c_messages;
		i2c_packets.nmsgs = num_xfers;
		status = ioctl(dev_fd, I2C_RDWR, &i2c_packets);
	}
#if DEBUG2
	if (direction != DIRECTION_SEND)
	{
		int j;
		fprintf(stderr, " <-");
		for (j = 0; j < data_len; j++)
			fprintf(stderr, " %02x", data[j]);
		fprintf(stderr, "\n");
	}
#endif
	if (status < 0)
		perror("message");
	return status >= 0;
}

static uint32_t be_4bytes(uint8_t *buffer)
{
	return buffer[3] + 0x100 * buffer[2] + 0x10000 * buffer[1] + 0x1000000 * buffer[0];
}

static void to_be_4bytes(uint32_t val, uint8_t *buffer)
{
	buffer[3] = val & 0xFF;
	buffer[2] = (val & 0xFF00) >> 8;
	buffer[1] = (val & 0xFF0000) >> 16;
	buffer[0] = (val & 0xFF000000) >> 24;
}

int open_device(char *dev_name, int mode, int addr)
{
	DEBUG(fprintf(stderr, "Open device\n"));
	if (dev_name == 0)
		dev_name = DEFAULT_SPI_DEV;
	i2c_addr = addr;
	dev_fd = open(dev_name, O_RDWR);
	if (dev_fd < 0)
		perror("open_device");
	return 1;
}

int check_device_id(uint32_t expected_id)
{
	uint8_t buffer[4];
	int status;
	DEBUG(fprintf(stderr, "Check device ID\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	status = send_receive(IDCODE_PUB, 0, DIRECTION_RECEIVE, buffer, 4);
	if (status != 1)
		return status;
	return be_4bytes(buffer) == expected_id;
}

int check_device_id_quick()
{
	uint8_t buffer[4];
	uint32_t device_id;
	int status;
	DEBUG(fprintf(stderr, "Check device ID (quick and dirty)\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	status = send_receive(IDCODE_PUB, 0, DIRECTION_RECEIVE, buffer, 4);
	if (status != 1)
		return status;
	device_id = be_4bytes(buffer);
	if (device_id == 0 || device_id == 0xFFFFFFFF)
	{
		fprintf(stderr, "Device ID = %04x\n", device_id);
		return 0;
	}
	else
		return 1;
}

int read_busy_status()
{
	uint8_t buffer[1];
	int status;
	DEBUG(fprintf(stderr, "Read busy status\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	status = send_receive(LSC_CHECK_BUSY, 0, DIRECTION_RECEIVE, buffer, 1);
	if (status != 1)
		return 0;
	DEBUG(fprintf(stderr, "Status: %02x\n", buffer[0]));
	return buffer[0] != 0;
}

int read_status_register()
{
	uint8_t buffer[4];
	uint32_t read_status;
	int status;
//  DEBUG(fprintf(stderr, "Read status register\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	status = send_receive(LSC_READ_STATUS, 0, DIRECTION_RECEIVE, buffer, 4);
	read_status = be_4bytes(buffer);
	DEBUG(fprintf(stderr, "Status: %04x %02x\n", read_status, buffer[2]));
	return READ_STATUS_BUSY(read_status) | READ_STATUS_FAIL(read_status);
}

int wait_not_busy()
{
	uint32_t status;
	DEBUG(fprintf(stderr, "Wait not busy\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	usleep(1000);
	while (read_busy_status())
		usleep(1000);
	while (status = read_status_register())
	{
		if (READ_STATUS_FAIL(status))
			return status;
		if (!READ_STATUS_BUSY(status))
			break;
	}
	return 1;
}

int erase_flash()
{
	int status;
	int i;
	DEBUG(fprintf(stderr, "Erase flash\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	status = send_receive(ISC_ERASE, ERASE_FEATURE_ROW | ERASE_CONFIGURATION | ERASE_USER_FLASH, DIRECTION_RECEIVE, 0, 0);
	return status;
}

int enable_offline_configuration()
{
	DEBUG(fprintf(stderr, "Enable offline configuration\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	return send_receive(ISC_ENABLE, 0x080000, DIRECTION_RECEIVE, 0, 0); /* TODO: special command for i2c */
}

int erase_user_flash()
{
	DEBUG(fprintf(stderr, "Erase user flash\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	return send_receive(LSC_ERASE_TAG, 0, DIRECTION_RECEIVE, 0, 0);
}

int set_configuration_flash_address(uint16_t page_address, int is_user_flash)
{
	uint8_t buffer[4];
	uint32_t address = page_address;
	DEBUG(fprintf(stderr, "Set configuration flash address\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	if (is_user_flash)
		address |= 0x40000000;
	to_be_4bytes(address, buffer);
	return send_receive(LSC_WRITE_ADDRESS, 0, DIRECTION_SEND, buffer, 4);
}

int reset_configuration_flash_address()
{
	DEBUG("Reset flash address\n");
	if (dev_fd == -1)
		return 1; // Debug mode
	return send_receive(LSC_INIT_ADDRESS, 0, DIRECTION_RECEIVE, 0, 0);
}

int program_configuration_flash(uint8_t *data, int data_len)
{
	DEBUG(fprintf(stderr, "Program flash\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	return send_receive(LSC_PROG_INCR_NV, 1, DIRECTION_SEND, data, data_len);
}

int program_user_code(uint32_t user_code)
{
	uint8_t buffer[4];
	DEBUG(fprintf(stderr, "Program user code\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	to_be_4bytes(user_code, buffer);
	return send_receive(ISC_PROGRAM_USERCODE, 0, DIRECTION_SEND, buffer, 4);
}

int verify_user_code(uint32_t expected_user_code)
{
	uint8_t buffer[4];
	int status;
	uint32_t user_code;
	DEBUG(fprintf(stderr, "Verify user code\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	status = send_receive(USERCODE, 0, DIRECTION_RECEIVE, buffer, 4);
	if (status != 1)
		return status;
	user_code = be_4bytes(buffer);
	if (user_code != expected_user_code)
	{
		fprintf(stderr, "Found %08x Expected %08x\n", user_code, expected_user_code);
		return 0;
	}
	return 1;
}

int verify_configuration_flash(uint8_t *expected_data, int data_len)
{
	uint8_t *data;
	int read_len;
	int status;
	uint32_t op;
	int read_idx, data_idx;
	DEBUG(fprintf(stderr, "Verify flash\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	if (data_len > MACHXO2_PAGE_SIZE)
	{
		if (mode == MODE_SPI)
		{
			read_len = data_len + MACHXO2_PAGE_SIZE; // One extra page
			read_idx = MACHXO2_PAGE_SIZE;
		}
		else
		{
			read_len = 2*MACHXO2_PAGE_SIZE + (data_len / MACHXO2_PAGE_SIZE) * 4 + data_len;
			read_idx = 2*MACHXO2_PAGE_SIZE;
		}
		op = data_len / MACHXO2_PAGE_SIZE + 1;
	}
	else
	{
		read_len = data_len;
		op = 1;
		read_idx = 0;
	}
	if (mode != MODE_I2C)
		op |= 0x100000;
	data = (uint8_t*)malloc(read_len);
	if (data == 0)
	{
		fprintf(stderr, "Malloc failed\n");
		return 0;
	}
	status = send_receive(LSC_READ_INCR_NV, op, DIRECTION_RECEIVE, data, read_len);
	if (status != 1)
		return status;
	data_idx = 0;
	while (data_idx < data_len)
	{
		if (data[read_idx] != expected_data[data_idx])
		{
			fprintf(stderr, "Verify failed at offset %d (%d) : found = %02x expected = %02x\n",
				data_idx, read_idx, data[read_idx], expected_data[data_idx]);
			return 0; // differs
		}
		data_idx++;
		read_idx++;
		if (((data_idx % MACHXO2_PAGE_SIZE) == 0) && (mode == MODE_I2C))
			read_idx += 4;
	}
	return 1;
}

int program_feature_row(uint8_t *feature_row)
{
	DEBUG(fprintf(stderr, "Program feature row\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	return send_receive(LSC_PROG_FEATURE, 0, DIRECTION_SEND, feature_row, 8);
}

int verify_feature_row(uint8_t *expected_feature_row)
{
	uint8_t buffer[8];
	int i;
	int status;
	DEBUG(fprintf(stderr, "Verify feature row\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	status = send_receive(LSC_READ_FEATURE, 0, DIRECTION_RECEIVE, buffer, 8);
	if (status != 1)
		return status;
	for (i = 0; i < 8; i++)
		if (buffer[i] != expected_feature_row[i])
			return 0;
	return 1;
}

int program_feature_bits(uint8_t *feature_bits)
{
	DEBUG(fprintf(stderr, "Program feature bits\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	return send_receive(LSC_PROG_FEABITS, 0, DIRECTION_SEND, feature_bits, 2);
}

int verify_feature_bits(uint8_t *expected_feature_bits)
{
	uint8_t buffer[2];
	int status;
	DEBUG(fprintf(stderr, "Verify feature bits\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	status = send_receive(LSC_READ_FEABITS, 0, DIRECTION_RECEIVE, buffer, 2);
	if (status != 1)
		return status;
	return buffer[0] == expected_feature_bits[0] && buffer[1] == expected_feature_bits[1];
}

int program_done()
{
	DEBUG(fprintf(stderr, "Program DONE\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	return send_receive(ISC_PROGRAM_DONE, 0, DIRECTION_RECEIVE, 0, 0);
}

int refresh()
{
	DEBUG(fprintf(stderr, "Refresh device\n"));
	if (dev_fd == -1)
		return 1; // Debug mode
	return send_receive(LSC_REFRESH, 0, DIRECTION_RECEIVE, 0, 0);
}
