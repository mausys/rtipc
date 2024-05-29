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
    bool b;
    uint8_t u8;
    int8_t i8;
    uint16_t u16;
    int16_t i16;
    uint32_t u32;
    int32_t i32;
    uint64_t u64;
    int64_t i64;
    float f32;
    double f64;
} generic_t;

typedef enum {
    COMMAND_ID_NOP,
    COMMAND_ID_GET,
    COMMAND_ID_SET,
} command_id_t;

typedef struct {
    uint64_t timestamp;
    uint8_t cmd;
    uint32_t cookie;
    uint8_t channel;
    uint8_t index;
    uint8_t subindex;
    generic_t value;
} command_t;

typedef struct {
    uint64_t timestamp;
    uint32_t cookie;
    generic_t value;
} response_t;

ri_odb_t *myobjects_create_odb(void);

static inline ri_object_meta_t create_object_meta(object_type_t type, uint8_t channel, uint8_t index, uint8_t array_len)
{
    if (array_len == 0)
        array_len = 1;

    ri_object_meta_t meta = {0};

    ri_object_id_t id = ((ri_object_id_t)type & 0xff) << 24;

    id |= ((ri_object_id_t)channel) << 16;
    id |= ((ri_object_id_t)index) << 8;
    id |= ((ri_object_id_t)array_len);

    switch (type) {
        case OBJECT_TYPE_U8:
            meta = RI_OBJECT_ARRAY_ID(id, uint8_t, array_len);
            break;
        case OBJECT_TYPE_I8:
            meta = RI_OBJECT_ARRAY_ID(id, int8_t, array_len);
            break;
        case OBJECT_TYPE_U16:
            meta = RI_OBJECT_ARRAY_ID(id, uint16_t, array_len);
            break;
        case OBJECT_TYPE_I16:
            meta = RI_OBJECT_ARRAY_ID(id, int16_t, array_len);
            break;
        case OBJECT_TYPE_U32:
            meta = RI_OBJECT_ARRAY_ID(id, uint32_t, array_len);
            break;
        case OBJECT_TYPE_I32:
            meta = RI_OBJECT_ARRAY_ID(id, int32_t, array_len);
            break;
        case OBJECT_TYPE_U64:
            meta = RI_OBJECT_ARRAY_ID(id, uint64_t, array_len);
            break;
        case OBJECT_TYPE_I64:
            meta = RI_OBJECT_ARRAY_ID(id, int64_t, array_len);
            break;
        case OBJECT_TYPE_F32:
            meta = RI_OBJECT_ARRAY_ID(id, float, array_len);
            break;
        case OBJECT_TYPE_F64:
            meta = RI_OBJECT_ARRAY_ID(id, double, array_len);
            break;
    }
    return meta;
}


static inline object_type_t object_id_get_type(const ri_object_id_t *id)
{
    return (object_type_t)(((*id) >> 24) & 0xff);
}
