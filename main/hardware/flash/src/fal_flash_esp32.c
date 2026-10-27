/*
 * Copyright (c) 2022, kaans, <https://github.com/kaans>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include <esp_partition.h>

#include <string.h>
#include <fal.h>

#define FLASH_ERASE_MIN_SIZE (4 * 1024)

#define LOCKER_ENABLE
#ifdef LOCKER_ENABLE
    #include "freertos/FreeRTOS.h"
    #include "freertos/semphr.h"

    static SemaphoreHandle_t s_lock = NULL;

    const static esp_partition_t *part_kv, *part_ts;

    //if sem not init, then do so
    #define LOCK()                                 \
        do                                         \
        {                                          \
            if(!s_lock){s_lock=xSemaphoreCreateMutex();}\
            xSemaphoreTake(s_lock, portMAX_DELAY); \
        } while (0)

    #define UNLOCK()                \
        do                          \
        {                           \
            xSemaphoreGive(s_lock); \
        } while (0)
#else
    #define create_LOCK()
    #define LOCK()
    #define UNLOCK()
#endif

static int init(void)
{
    ESP_LOGI("flash", "flash init");
    ESP_LOGI("flash", "cur part:");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL) {
        const esp_partition_t *p = esp_partition_get(it);
        ESP_LOGI("flash", "part: partname='%s', type=0x%02x, subtype=0x%02x, offset=0x%" PRIx32 ", size=0x%" PRIx32,
                 p->label, p->type, p->subtype, p->address, p->size);
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
#ifdef LOCKER_ENABLE
    if (s_lock == NULL)
    {
        s_lock = xSemaphoreCreateCounting(1, 1);
        assert(s_lock != NULL);
    }
#endif

    // the values passed to esp_partition_find_first() must correspond to the
    // values set in partitions.csv for the partition named "flashdb".
    //locate fdb_kv分区
    part_kv = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x99, "fdb_kv");//locate 
    part_ts = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x9a, "fdb_ts");//locate 
    if(part_kv==NULL)
    {
        ESP_LOGE("flash", "KV init fail");
        return 0;
    }
    if(part_ts==NULL)
    {
        ESP_LOGE("flash", "TS init fail");
        return 0;
    }
    assert(partition != NULL);//何意味啊？

    return 1;
}

static int read(long offset, uint8_t *buf, size_t size)
{
    esp_err_t err;

    LOCK();
    if (offset < FALSH_KV_SIZE) 
    {
        //get data from flash fdb_kv，val store to buf
        err = esp_partition_read(part_kv, offset, buf, size);
    } else {
        //get data from flash fdb_ts, easyflash treat it as 1 分区 , hardware treat it as 2
        err = esp_partition_read(part_ts, offset - FALSH_KV_SIZE, buf, size);
    }
    UNLOCK();
    // ESP-IDF 返回 ESP_OK 表示成功，按 FAL 規範返回實際讀取字節數
    return (err == ESP_OK) ? size : -1;
}

static int write(long offset, const uint8_t *buf, size_t size)
{
    esp_err_t err;

    LOCK();
    if (offset < FALSH_KV_SIZE) {
        //write to kv part
        err = esp_partition_write(part_kv, offset, buf, size);
    } else {
        //wrtie to ts part
        err = esp_partition_write(part_ts, offset - FALSH_KV_SIZE, buf, size);
    }

    UNLOCK();
    return (err == ESP_OK) ? size : -1;
}

static int erase(long offset, size_t size)
{
    esp_err_t err;
    int32_t erase_size = ((size - 1) / FLASH_ERASE_MIN_SIZE) + 1;

    LOCK();
    if (offset < FALSH_KV_SIZE) {
        //erase kv
        err = esp_partition_erase_range(part_kv, offset, erase_size*FLASH_ERASE_MIN_SIZE);
    } else {
        //erase ts
        err = esp_partition_erase_range(part_ts, offset - FALSH_KV_SIZE, erase_size*FLASH_ERASE_MIN_SIZE);
    }
    UNLOCK();

    return (err == ESP_OK) ? size : -1;
}

const struct fal_flash_dev nor_flash0 =
    {
        .name = NOR_FLASH_DEV_NAME,
        .addr = 0x0,                      // address is relative to beginning of partition; 0x0 is start of the partition
        .len = 2048 * 1024,                 // size of the partition as specified in partitions.csv
        .blk_size = FLASH_ERASE_MIN_SIZE, // must be 4096 bytes
        .ops = {init, read, write, erase},
        .write_gran = 1, // 1 byte write granularity
};
