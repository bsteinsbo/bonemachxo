/*
 * Functions for JEDEC files for Lattice MachXO2 FPGA's.
 * Copyright (c) 2013 Bjarne Steinsbo <bjarne at steinsbo dot com>
 * License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "jedec.h"

static FILE *f = 0;

static int is_ws(int c)
{
	return (c == '\n' || c == ' ' || c == '\r' || c == '\t');
}

static int bitstring_to_bytes(uint8_t *data)
{
	int dst_idx = 0;
	int cur_bit = 0;
	int src_idx;
	for (src_idx = 0; data[src_idx] != 0; src_idx++)
	{
		int bit = data[src_idx];
		if (bit == '0')
			bit = 0;
		else if (bit == '1')
			bit = 1;
		else
			continue;
		if (cur_bit == 8)
		{
			cur_bit = 0;
			dst_idx++;
		}
		if (cur_bit == 0)
			data[dst_idx] = bit; // Initialize with zero
		else
			data[dst_idx] = (data[dst_idx] << 1) | bit;
		cur_bit++;
	}
	return dst_idx + 1;
}

static int reverse_bits(uint8_t *data, int data_len)
{
  int i, j;
  // reverse bytes
  for (i = 0; i < data_len / 2; i++)
  {
    uint8_t tmp = data[data_len - i - 1];
    data[data_len - i - 1] = data[i];
    data[i] = tmp;
  }
  // reverse bits
  for (i = 0; i < data_len; i++)
  {
    uint8_t tmp = data[i];
    data[i] = 0;
    for (j = 0; j < 8; j++)
    {
      data[i] = (data[i] << 1) | (tmp & 1);
      tmp >>= 1;
    }
  }
}

int open_jedec(char *fname)
{
	f = fopen(fname, "r");
	if (f == 0)
	{
		perror("open_jedec");
		return 0;
	}
	while (!feof(f))
	{
		int c = getc(f);
		if (c == '\x02')
			break;
	}
	if (feof(f))
	{
		fprintf(stderr, "Could not find start of file marker\n");
		return 0;
	}
	return 1;
}

int get_next_jedec_section(int *section, uint32_t *address, uint8_t **data, int *data_len)
{
	int c;
	int buffer_free;
	uint8_t *buffer;
	int buffer_ptr;
	int i;
	// Defaults
	*address = 0;
	*data = 0;
	*data_len = 0;
	while (!feof(f))
	{
		int c = getc(f);
		if (!is_ws(c))
		{
			ungetc(c, f);
			break;
		}
	}
	if (feof(f))
	{
		fprintf(stderr, "Truncated file\n");
		return 0;
	}
	c = getc(f);
	if (c == '\x03')
	{
		// End of file
		*section = SECTION_END;
		return 1;
	}
	else if (c == '*')
	{
		// empty section
		*section = SECTION_NONE;
		return 1;
	}
	// Assume valid JEDEC code.  Read data (skip white space)
	buffer_free = 80;
	buffer = (uint8_t*)malloc(buffer_free + 1);
	if (buffer == 0)
	{
		fprintf(stderr, "Out of memory\n");
		return 0;
	}
	buffer_ptr = 0;
	while (!feof(f))
	{
		int c = getc(f);
		if (c == '*')
			break;
		if (buffer_free == 0)
		{
			buffer_free = 2048;
			buffer = (uint8_t*)realloc(buffer, buffer_ptr + buffer_free + 1);
			if (buffer == 0)
			{
				fprintf(stderr, "Out of memory\n");
				return 0;
			}
		}
		buffer[buffer_ptr++] = c;
		--buffer_free;
	}
	buffer[buffer_ptr] = 0;
	if (feof(f))
	{
		fprintf(stderr, "Unexpected end of file\n");
		return 0;
	}
	switch (c)
	{
	case 'N':
		*section = SECTION_NOTE;
		*data = buffer;
		*data_len = buffer_ptr;
		return 1;
	case 'Q':
		if (buffer[0] == 'F')
		{
			*section = SECTION_NUM_FUSES;
			*data = buffer + 1;
			*data_len = buffer_ptr - 1;
		}
		else if (buffer[0] = 'P')
		{
			*section = SECTION_NUM_PINS;
			*data = buffer + 1;
			*data_len = buffer_ptr - 1;
		}
		else
		{
			fprintf(stderr, "Unknow field 'Q%c'\n", buffer[0]);
			return 0;
		}
		return 1;
	case 'F':
		*section = SECTION_DEFAULT_FUSE_VAL;
		*data = buffer;
		*data_len = 1;
		return 1;
	case 'G':
		*section = SECTION_SECURITY_FUSE;
		*data = buffer;
		*data_len = 1;
		return 1;
	case 'L':
		*section = SECTION_FUSE_MAP;
		*address = (uint32_t)strtol((char *)buffer, 0, 10);
		for (i = 0; buffer[i] != '\n' && buffer[i] != 0; i++)
			;
		if (buffer[i] == 0)
		{
			fprintf(stderr, "Unexpected end of file\n");
			return 0;
		}
		*data = buffer + i + 1;
		*data_len = bitstring_to_bytes(*data);
		return 1;
	case 'C':
		*section = SECTION_CHECK_SUM;
		*data = buffer;
		*data_len = 4;
		return 1;
	case 'U':
		*section = SECTION_USERCODE;
		if (buffer[0] == 'A')
		{
			*address = buffer[4] + 0x100 * buffer[3] + 0x10000 * buffer[2] + 0x1000000 * buffer[1];
		}
		else if (buffer[0] == 'H')
		{
			*address = (uint32_t)strtol((char *)(buffer + 1), 0, 16);
		}
		else
		{
			bitstring_to_bytes(buffer);
			*address = buffer[3] + 0x100 * buffer[2] + 0x10000 * buffer[1] + 0x1000000 * buffer[0];
		}
		return 1;
	case 'E':
		*section = SECTION_ARCH;
		*data = buffer;
		*data_len = bitstring_to_bytes(buffer);
    /* Incredibly enough, Lattice has managed to reverse the bits for features... */
    if (*data_len == 10)
    {
      reverse_bits(*data, 8);
      reverse_bits(*data + 8, 2);
    }
    else
    {
      fprintf(stderr, "Unexpected data length '%d' for feature row/bits.  Expected 10 bytes.\n", *data_len);
      return 0;
    }
		return 1;
	default:
		fprintf(stderr, "Unknown field '%c'\n", c);
		break;
	}
	return 0;
}