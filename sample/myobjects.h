#pragma once

#include <rtipc/object.h>
#include <rtipc/odb.h>

typedef enum {
  C2S_CHANNEL_ID_COMMAND = 0,
  C2S_CHANNEL_ID_RPDO_1,
  C2S_CHANNEL_ID_RPDO_2,
  C2S_CHANNEL_ID_LAST = C2S_CHANNEL_ID_RPDO_2,
} c2s_channel_id_t;


typedef enum {
    S2C_CHANNEL_ID_RESPONSE = 0,
    S2C_CHANNEL_ID_TPDO_1,
    S2C_CHANNEL_ID_TPDO_2,
    S2C_CHANNEL_ID_LAST = S2C_CHANNEL_ID_TPDO_2,
} s2c_channel_id_t;


typedef struct {
    uint32_t seqno;
    uint32_t timestamp;
} header_t;

typedef enum {
    OBJECT_TYPE_U8 = 0,
    OBJECT_TYPE_I8,
    OBJECT_TYPE_U16,
    OBJECT_TYPE_I16,
    OBJECT_TYPE_U32,
    OBJECT_TYPE_I32,
    OBJECT_TYPE_U64,
    OBJECT_TYPE_I64,
    OBJECT_TYPE_F32,
    OBJECT_TYPE_F64,
    OBJECT_TYPE_LAST = OBJECT_TYPE_F64
} object_type_t;


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
    header_t header;
    uint32_t cmd;
    generic_t arg1;
    generic_t arg2;
    generic_t arg3;
} command_t;

ri_odb_t *myobjects_create_odb(void);

static inline ri_object_id_t create_object_id(object_type_t type, unsigned channel, unsigned index, unsigned array_len)
{
    ri_object_id_t id = ((ri_object_id_t)type & 0xff) << 24;
    id |= ((ri_object_id_t)channel & 0xff) << 16;
    id |= ((ri_object_id_t)index & 0xff) << 8;
    id |= ((ri_object_id_t)array_len & 0xff);
    return id;
}


static inline object_type_t object_id_get_type(const ri_object_id_t *id)
{
    return (object_type_t)(((*id) >> 24) & 0xff);
}
