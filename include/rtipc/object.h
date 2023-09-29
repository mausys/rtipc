#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <rtipc/channel.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct ri_obj_desc {
    void *p;
    size_t size;
    size_t align;
} ri_obj_desc_t;

typedef struct ri_rom ri_rom_t;
typedef struct ri_tom ri_tom_t;

#define RI_OBJECT(x) (ri_obj_desc_t) { .p = &(x), .size = sizeof(*(x)), .align = __alignof__(*(x)) }
#define RI_OBJECT_ARRAY(x, s) (ri_obj_desc_t) { .p = &(x), .size = sizeof(*(x)) * (s), .align = __alignof__(*(x)) }
#define RI_OBJECT_NULL(s, a) (ri_obj_desc_t) { .p = NULL, .size = (s) , .align = a }
#define RI_OBJECT_END (ri_obj_desc_t) { .p = NULL, .size = 0 }


size_t ri_calc_buffer_size(const ri_obj_desc_t descs[]);

ri_rom_t* ri_rom_new(const ri_rchn_t *chn, const ri_obj_desc_t *descs);

void ri_rom_delete(ri_rom_t* rom);

ri_tom_t* ri_tom_new(const ri_tchn_t *chn, const ri_obj_desc_t *descs, bool cache);

void ri_tom_delete(ri_tom_t* tom);

void ri_tom_update(ri_tom_t *tom);

bool ri_tom_ackd(const ri_tom_t *tom);

int ri_rom_update(ri_rom_t *rom);


#ifdef __cplusplus
}
#endif
