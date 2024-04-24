#pragma once

#include <rtipc/rtipc.h>
#include <rtipc/object.h>
#include <rtipc/mapper.h>


typedef enum {
    C2S_CHANNEL_ID_COMMAND,
    C2S_CHANNEL_SETPOINTS_1,
    C2S_CHANNEL_SETPOINTS_2
} c2s_channel_id_t;


typedef enum {
    S2C_CHANNEL_ID_RESPONSE,
    S2C_CHANNEL_PROCESS_VARIABLES_1,
    S2C_CHANNEL_PROCESS_VARIABLES_2,
    S2C_CHANNEL_PROCESS_VARIABLES_3,
} s2c_channel_id_t;


typedef enum {
    OBJECT_TYPE_BOOL,
    OBJECT_TYPE_U8,
    OBJECT_TYPE_I8,
    OBJECT_TYPE_U16,
    OBJECT_TYPE_I16,
    OBJECT_TYPE_U32,
    OBJECT_TYPE_U64,
    OBJECT_TYPE_FLOAT,
    OBJECT_TYPE_DOUBLE,
    OBJECT_TYPE_OTHER,
} object_type_t;


typedef struct {
    uint64_t timestamp;
    uint32_t seqno;
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

typedef enum {
    CMD_SET,
    CMD_GET,
} cmd_id_t;

typedef struct command {
    header_t header;
    cmd_id_t cid;
    uint32_t cookie;
    ri_object_id_t oid;
    generic_t value;
} command_t;


typedef struct response {
    header_t header;
    uint32_t cookie;
    ri_object_id_t oid;
    generic_t value
} response_t;




static inline ri_object_id_t object_id(uint32_t type, unsigned no)
{
    ri_object_id_t id = ((ri_object_id_t)(type)) << 32;
    id |= ((ri_object_id_t)(no & 0xffffffff));
    return id;
}
