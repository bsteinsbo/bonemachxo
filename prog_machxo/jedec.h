/*
 * Definitions for JEDEC files for Lattice MachXO2 FPGA's.
 * Copyright (c) 2013 Bjarne Steinsbo <bjarne at steinsbo dot com>
 * License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
 */
#ifndef _JEDEC_H
#define _JEDEX_H 1

#define SECTION_NONE 0
#define SECTION_END 1
#define SECTION_NOTE 2
#define SECTION_NUM_PINS 3
#define SECTION_NUM_FUSES 4
#define SECTION_DEFAULT_FUSE_VAL 5
#define SECTION_SECURITY_FUSE 6
#define SECTION_FUSE_MAP 7
#define SECTION_CHECK_SUM 8
#define SECTION_ARCH 9
#define SECTION_USERCODE 10

int open_jedec(char *fname);
int get_next_jedec_section(int *section, uint32_t *address, uint8_t **data, int *data_len);

#endif