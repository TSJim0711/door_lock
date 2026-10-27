/*
 * Copyright (c) 2022, kaans, <https://github.com/kaans>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

//#define FAL_DEBUG 1
#define FAL_PART_HAS_TABLE_CFG
#define NOR_FLASH_DEV_NAME "norflash0"

//#define FAL_PART_TABLE_FLASH_DEV_NAME NOR_FLASH_DEV_NAME
//#define FAL_PART_TABLE_END_OFFSET      65536

/* ===================== Flash device Configuration ========================= */
extern const struct fal_flash_dev nor_flash0;

/* flash device table */
#define FAL_FLASH_DEV_TABLE \
{                           \
    &nor_flash0,            \
}
/* ====================== Partition Configuration ========================== */
#ifdef FAL_PART_HAS_TABLE_CFG
/* partition table */
#define FAL_KV_PART_NAME "fdb_kv"
#define FAL_TS_PART_NAME "fdb_ts"
#define FAL_PART_TABLE                                                               \
{                                                                                    \
    {FAL_PART_MAGIC_WORD, FAL_KV_PART_NAME, NOR_FLASH_DEV_NAME, 0,              1024 * 1024, 0}, \
    {FAL_PART_MAGIC_WORD, FAL_TS_PART_NAME, NOR_FLASH_DEV_NAME, 1024 * 1024,    1024 * 1024, 0} \
}//lagacy db 1M, for log 1M
#define FALSH_KV_SIZE (1024*1024)

#endif /* FAL_PART_HAS_TABLE_CFG */

#endif /* _FAL_CFG_H_ */
