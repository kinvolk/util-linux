/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "blkidP.h"

#define LVM2_ID_LEN 32

struct lvm2_pv_label_header {
	/* label_header */
	uint8_t		id[8];		/* LABELONE */
	uint64_t	sector_xl;	/* Sector number of this label */
	uint32_t	crc_xl;		/* From next field to end of sector */
	uint32_t	offset_xl;	/* Offset from start of struct to contents */
	uint8_t		type[8];	/* LVM2 001 */
	/* pv_header */
	uint8_t		pv_uuid[LVM2_ID_LEN];
} __attribute__ ((packed));

#define LVM2_LABEL_SIZE 512
static unsigned int lvm2_calc_crc(const void *buf, unsigned int size)
{
	static const unsigned int crctab[] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	unsigned int i, crc = 0xf597a6cf;
	const uint8_t *data = (const uint8_t *) buf;

	for (i = 0; i < size; i++) {
		crc ^= *data++;
		crc = (crc >> 4) ^ crctab[crc & 0xf];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
	}
	return crc;
}

static int probe_lvm2(blkid_probe pr, const struct blkid_idmag *mag)
{
	int sector = mag->kboff << 1;
	struct lvm2_pv_label_header *label;
	char *p, *q, uuid[40];
	unsigned int i, b;
	unsigned char *buf;

	buf = blkid_probe_get_buffer(pr,
			mag->kboff << 10,
			512 + sizeof(struct lvm2_pv_label_header));
	if (!buf)
		return -1;

	/* buf is at 0k or 1k offset; find label inside */
	if (memcmp(buf, "LABELONE", 8) == 0) {
		label = (struct lvm2_pv_label_header *) buf;
	} else if (memcmp(buf + 512, "LABELONE", 8) == 0) {
		label = (struct lvm2_pv_label_header *)(buf + 512);
		sector++;
	} else {
		return 1;
	}

	if (le64_to_cpu(label->sector_xl) != (unsigned) sector)
		return 1;

	if (lvm2_calc_crc(&label->offset_xl, LVM2_LABEL_SIZE -
			((char *) &label->offset_xl - (char *) label)) !=
			le32_to_cpu(label->crc_xl)) {
		DBG(DEBUG_PROBE,
		    printf("LVM2: label checksum incorrect at sector %d\n",
			   sector));
		return 1;
	}

	for (i = 0, b = 1, p = uuid, q = (char *) label->pv_uuid; i <= 32;
	     i++, b <<= 1) {
		if (b & 0x4444440)
			*p++ = '-';
		*p++ = *q++;
	}

	uuid[LVM2_ID_LEN+6] = '\0';
	blkid_probe_sprintf_uuid(pr, label->pv_uuid, sizeof(label->pv_uuid),
			"%s", uuid);
	return 0;
}


/* NOTE: the original libblkid uses "lvm2pv" as a name */
const struct blkid_idinfo lvm2_idinfo =
{
	.name		= "LVM2_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_lvm2,
	.magics		=
	{
		{ .magic = "LVM2 001", .len = 8, .sboff = 0x218 },
		{ .magic = "LVM2 001", .len = 8, .sboff = 0x018 },
		{ .magic = "LVM2 001", .len = 8, .kboff = 1, .sboff = 0x018 },
		{ .magic = "LVM2 001", .len = 8, .kboff = 1, .sboff = 0x218 },
		{ NULL }
	}
};

const struct blkid_idinfo lvm1_idinfo =
{
	.name		= "LVM1_member",
	.usage		= BLKID_USAGE_RAID,
	.magics		=
	{
		{ .magic = "HM", .len = 2, .sboff = 0x400 },
		{ NULL }
	}
};


