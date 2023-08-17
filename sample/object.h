#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t seqno;
    uint32_t timestamp;
} object_header_t;


typedef struct
{
    object_header_t header;
    uint32_t id;
} object_cmd_t;


typedef union
{
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
} object_arg_t;


typedef struct
{
    object_header_t header;
    int ret;
} object_rsp_t;


typedef struct
{
    object_header_t header;
    uint8_t data;
} object_u8_t;


typedef struct
{
    object_header_t header;
    int8_t data;
} object_s8_t;


typedef struct
{
    object_header_t header;
    uint16_t data;
} object_u16_t;


typedef struct
{
    object_header_t header;
    int16_t data;
} object_s16_t;


typedef struct
{
    object_header_t header;
    uint32_t data;
} object_u32_t;


typedef struct
{
    object_header_t header;
    int32_t data;
} object_s32_t;


typedef struct
{
    object_header_t header;
    uint64_t data;
} object_u64_t;


typedef struct
{
    object_header_t header;
    int64_t data;
} object_s64_t;


typedef struct
{
    object_header_t header;
    float data;
} object_f32_t;


typedef struct
{
    object_header_t header;
    double data;
} object_f64_t;



