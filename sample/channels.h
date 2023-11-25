#pragma once

#include <rtipc.h>


typedef struct {
    uint32_t seqno;
    uint32_t timestamp;
} header_t;


typedef union {
    uint8_t u8;
    int8_t s8;
    uint16_t u16;
    int16_t s16;
    uint32_t u32;
    int32_t s32;
    uint64_t u64;
    int64_t s64;
    float f32;
    double f64;
} generic_t;


typedef struct
{
    header_t *header;
    uint32_t *cmd;
    generic_t *arg1;
    generic_t *arg2;
    generic_t *arg3;
} channel_cmd_t;


typedef struct {
    header_t *header;
    uint32_t *cmd;
    generic_t *data1;
    generic_t *data2;
    generic_t *data3;
} channel_rsp_t;


typedef struct {
    header_t *header;
    double *data1;
    uint16_t *data2;
    uint8_t *data3;
    uint64_t *data4;
} channel_rpdo1_t;


typedef struct {
    header_t *header;
    uint8_t *data1;
    uint64_t* data2;
    uint8_t* data3;
} channel_rpdo2_t;


typedef struct {
    header_t *header;
    uint8_t *data1;
    double *data2;
    uint16_t* data3;
} channel_tpdo1_t;



void channel_cmd_to_objects(channel_cmd_t *channel, ri_object_t  *objects);
void channel_rsp_to_objects(channel_rsp_t *channel, ri_object_t  *objects);
void channel_rpdo1_to_objects(channel_rpdo1_t *channel, ri_object_t  *objects);
void channel_rpdo2_to_objects(channel_rpdo2_t *channel, ri_object_t  *objects);
void channel_tpdo1_to_objects(channel_tpdo1_t *channel, ri_object_t  *objects);
