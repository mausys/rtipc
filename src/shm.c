#include "rtipc/rtipc.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdalign.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "mem_utils.h"
#include "sys.h"

#define MAGIC 0x1f0ca3be // lock-free zero-copy atomic triple buffer exchange :)

#define DIRECTION_MASK    0x1

#define LAST_CHANNEL_MASK 0x2

#define LAYOUT_NUM_SEGMENTS 6

typedef struct ri_shm_segment {
    size_t offset;
    size_t size;
} shm_segment_t;


typedef struct ri_group {
    unsigned num;
    shm_segment_t table;
    shm_segment_t data;
    shm_segment_t meta;
} shm_group_t;

typedef struct {
    size_t hdr_size;
    shm_group_t producers;
    shm_group_t consumers;
} shm_layout_t;



struct ri_shm {
    ri_sys_t *sys;
    shm_layout_t layout;
    ri_consumer_t *consumers;
    ri_producer_t *producers;
};


typedef struct {
    ri_xchg_t xchg;
    uint32_t data_offset;
    uint32_t buffer_size;
    uint32_t meta_offset;
    uint32_t meta_size;
} table_entry_t;


typedef struct {
    uint32_t num_c2s; /**< number of consumers from server perspective */
    uint32_t num_s2c; /**< number of producers from server perspective */
    uint32_t offset_s2c_table;
    uint32_t offset_c2s_table;
    uint32_t offset_s2c_data;
    uint32_t offset_c2s_data;
    uint32_t offset_s2c_meta;
    uint32_t offset_c2s_meta;
    uint16_t cach_line_size;
    uint8_t max_alignment;
    uint8_t xchg_size;
    uint32_t magic;
} shm_header_t;


static unsigned count_description_entries(const ri_channel_req_t entries[])
{
    if (!entries)
        return 0;

    unsigned i;

    for (i = 0; entries[i].buffer_size != 0; i++)
        ;

    return i;
}


static size_t table_entry_size(void)
{
    return mem_align(sizeof(table_entry_t), cacheline_size());
}

static size_t buffer_size(size_t min_size)
{
    return mem_align(min_size, cacheline_size());
}

static size_t channel_data_size(size_t buf_size)
{
    return RI_NUM_BUFFERS * buffer_size(buf_size);
}


static size_t channel_meta_size(size_t min_size)
{
    return mem_align(min_size, alignof(max_align_t));
}


static int init_channel(ri_channel_t *channel, void *ptr, const shm_group_t *group, table_entry_t *entry)
{
    channel->xchg = &entry->xchg;

    size_t offset =  0;

    for (int i = 0; i < RI_NUM_BUFFERS; i++) {
        channel->bufs[i] = mem_offset(ptr, group->data.offset + offset);
        offset += buffer_size(entry->buffer_size);
        if (offset > group->data.size)
            return -1;
    }

    if (entry->meta_offset + entry->meta_size > group->meta.size)
        return -1;

    channel->meta.size = entry->meta_size;
    channel->meta.ptr = entry->meta_size > 0 ? mem_offset(ptr, group->meta.offset + entry->meta_offset) : NULL;

    return 0;
}

static void delete_channels(ri_shm_t *shm)
{
    if (shm->producers) {
        free(shm->producers);
        shm->producers = NULL;
    }
    if (shm->consumers) {
        free(shm->consumers);
        shm->consumers = NULL;
    }
}

static int create_channels(ri_shm_t *shm)
{
    delete_channels(shm);
    int r = -1;

    const shm_layout_t *layout = &shm->layout;

    if (layout->consumers.num > 0) {
        shm->consumers = malloc(layout->consumers.num * sizeof(ri_consumer_t));

        if (!shm->consumers) {
            r = -ENOMEM;
            goto fail;
        }
    }

    if (layout->producers.num > 0) {
        shm->producers = malloc(layout->producers.num * sizeof(ri_producer_t));

        if (!shm->producers) {
            r = -ENOMEM;
            goto fail;
        }
    }

    for (unsigned i = 0; i < layout->producers.num; i++) {
        shm->producers[i].current = RI_BUFIDX_NONE;
        shm->producers[i].locked = RI_BUFIDX_NONE;
    }

    size_t offset = 0;

    void *ptr = shm->sys->ptr;

    for (unsigned i = 0; i < layout->consumers.num; i++) {
        table_entry_t *entry = mem_offset(ptr, layout->consumers.table.offset + offset);

        ri_channel_t *channel = &shm->consumers[i].channel;

        r = init_channel(channel, ptr, &layout->consumers, entry);

        if (r < 0)
            goto fail;

        offset += table_entry_size();

        if (offset > layout->consumers.table.size) {
            r = -1;
            goto fail;
        }

    }

    offset = 0;

    for (unsigned i = 0; i < layout->producers.num; i++) {
        table_entry_t *entry = mem_offset(ptr, layout->producers.table.offset + offset);

        ri_channel_t *channel = &shm->producers[i].channel;

        r = init_channel(channel, ptr, &layout->producers, entry);

        shm->producers[i].current = RI_BUFIDX_NONE;
        shm->producers[i].locked = RI_BUFIDX_NONE;

        if (r < 0)
            goto fail;

        offset += table_entry_size();
        if (offset > layout->producers.table.size) {
            r = -1;
            goto fail;
        }
    }

    return 0;

fail:
    delete_channels(shm);
    return r;
}


static int client_set_layout_sizes(shm_layout_t *layout, size_t shm_size)
{
    shm_segment_t *segments[LAYOUT_NUM_SEGMENTS] = {
        &layout->producers.table,
        &layout->consumers.table,

        &layout->producers.data,
        &layout->consumers.data,

        &layout->producers.meta,
        &layout->consumers.meta,
    };

    for (unsigned i = 0; i < LAYOUT_NUM_SEGMENTS - 1; i++) {
        if (segments[i]->offset > segments[i + 1]->offset) {
            LOG_ERR("client_set_layout_sizes: overlapping segments %u %u", i, i + 1);
            return -1;
        }

        segments[i]->size = segments[i + 1]->offset - segments[i]->offset;
    }

    shm_segment_t *last = segments[LAYOUT_NUM_SEGMENTS - 1];

    if (last->offset > shm_size) {
        LOG_ERR("client_set_layout_sizes: segments (%zu) exeed shm size (%zu)", last->offset, shm_size);
        return -1;
    }

    last->size = shm_size - last->offset;

    return 0;
}


static int validate_header(shm_header_t *header)
{
    if (header->magic != MAGIC) {
        LOG_ERR("validate_header: invalid magic=0x%x expected=0x%x", header->magic, MAGIC);
        return -1;
    }

    if (header->cach_line_size != cacheline_size()) {
        LOG_ERR("validate_header: cach_line_size disagreement server=%u client=%zu", header->cach_line_size, cacheline_size());
        return -1;
    }

    if (header->max_alignment != alignof(max_align_t)) {
        LOG_ERR("validate_header: max_alignment disagreement server=%u client=%zu", header->max_alignment, alignof(max_align_t));
        return -1;
    }

    return 0;
}

static int client_read_layout(const void *ptr, size_t shm_size, shm_layout_t *layout)
{
    shm_header_t header;
    memcpy(&header, ptr, sizeof(header));

    int r = validate_header(&header);
    if (r < 0)
        return r;

    *layout = (shm_layout_t) {
        .consumers.num = header.num_s2c,
        .consumers.table.offset = header.offset_s2c_table,
        .consumers.data.offset = header.offset_s2c_data,
        .consumers.meta.offset = header.offset_s2c_meta,

        .producers.num = header.num_c2s,
        .producers.table.offset = header.offset_c2s_table,
        .producers.data.offset = header.offset_c2s_data,
        .producers.meta.offset = header.offset_c2s_meta,
    };


    r = client_set_layout_sizes(layout, shm_size);
    if (r < 0)
        return r;

    return 0;
}


static int client_read_shm(ri_shm_t *shm)
{
    int r = client_read_layout(shm->sys->ptr, shm->sys->size, &shm->layout);

    if (r < 0)
        return r;

    return create_channels(shm);
}


static size_t server_set_segment_offset(shm_segment_t *segment, size_t offset)
{
    segment->offset = offset;
    return offset + segment->size;
}


static void server_set_group_sizes(shm_group_t *group, const ri_channel_req_t descs[])
{
    group->num = count_description_entries(descs);

    group->table.size = group->num * table_entry_size();

    for (unsigned i = 0; i < group->num; i++)
        group->data.size += channel_data_size(descs[i].buffer_size);

    for (unsigned i = 0; i < group->num; i++) {
        if (descs[i].meta.size)
            group->meta.size += channel_meta_size(descs[i].meta.size);
    }
}

static size_t server_set_segment_offsets(shm_layout_t *layout, size_t offset)
{
    shm_group_t *cns = &layout->consumers;
    shm_group_t *prd = &layout->producers;

    offset = server_set_segment_offset(&cns->table, offset);
    offset = server_set_segment_offset(&prd->table, offset);

    offset = server_set_segment_offset(&cns->data, offset);
    offset = server_set_segment_offset(&prd->data, offset);

    offset = server_set_segment_offset(&cns->meta, offset);
    offset = server_set_segment_offset(&prd->meta, offset);

    return offset;
}


static size_t server_init_layout(shm_layout_t *layout, const ri_channel_req_t consumers[], const ri_channel_req_t producers[])
{
    size_t offset = mem_align(sizeof(shm_header_t), cacheline_size());

    server_set_group_sizes(&layout->consumers, consumers);
    server_set_group_sizes(&layout->producers, producers);

    offset = server_set_segment_offsets(layout, offset);

    return offset;
}


static void server_write_header(void *ptr, const shm_layout_t *layout)
{
    shm_header_t header = {
        .num_c2s = layout->consumers.num,
        .offset_c2s_table = layout->consumers.table.offset,
        .offset_c2s_data = layout->consumers.data.offset,
        .offset_c2s_meta = layout->consumers.meta.offset,

        .num_s2c = layout->producers.num,
        .offset_s2c_table = layout->producers.table.offset,
        .offset_s2c_data = layout->producers.data.offset,
        .offset_s2c_meta = layout->producers.meta.offset,

        .max_alignment = alignof(max_align_t),
        .cach_line_size = cacheline_size(),
        .xchg_size = sizeof(ri_xchg_t),
        .magic = MAGIC,
    };

    memcpy(ptr, &header, sizeof(header));
}


static void server_write_group(void *ptr, const shm_group_t *group, const ri_channel_req_t channels[])
{
    void *table_ptr = mem_offset(ptr, group->table.offset);
    void *meta_ptr = mem_offset(ptr, group->meta.offset);

    size_t offset_table = 0;
    size_t data_offset = 0;
    size_t meta_offset = 0;

    for (unsigned i = 0; i < group->num; i++) {
        table_entry_t entry = {
            .xchg = RI_BUFIDX_NONE,
            .data_offset = data_offset,
            .meta_offset = meta_offset,
            .meta_size = channels[i].meta.size,
            .buffer_size = channels[i].buffer_size
        };

        memcpy(mem_offset(table_ptr, offset_table), &entry, sizeof(entry));

        if (channels[i].meta.size > 0 && channels[i].meta.ptr)
            memcpy(mem_offset(meta_ptr, meta_offset), channels[i].meta.ptr, channels[i].meta.size);

        offset_table += table_entry_size();
        data_offset += channel_data_size(channels[i].buffer_size);
        meta_offset += channel_meta_size(channels[i].meta.size);
    }
}


static ri_shm_t* create_shm(const ri_channel_req_t consumers[], const ri_channel_req_t producers[], const char *name, mode_t mode)
{
    ri_shm_t *shm = calloc(1, sizeof(ri_shm_t));

    if (!shm)
        goto fail_alloc;

    size_t shm_size = server_init_layout(&shm->layout, consumers, producers);

    if (name)
        shm->sys = ri_sys_named_new(shm_size, name, mode);
    else
        shm->sys = ri_sys_anon_new(shm_size);

    if (!shm->sys)
        goto fail_sys;

    server_write_header(shm->sys->ptr, &shm->layout);
    server_write_group(shm->sys->ptr, &shm->layout.consumers, consumers);
    server_write_group(shm->sys->ptr, &shm->layout.producers, producers);

    int r = create_channels(shm);

    if (r < 0)
        goto fail_channels;

    return shm;

fail_channels:
    ri_sys_delete(shm->sys);
fail_sys:
    free(shm);
fail_alloc:
    return NULL;
}


ri_shm_t* ri_anon_shm_new(const ri_channel_req_t consumers[], const ri_channel_req_t producers[])
{
    return create_shm(consumers, producers, NULL, 0);
}


ri_shm_t* ri_named_shm_new(const ri_channel_req_t consumers[], const ri_channel_req_t producers[], const char *name, mode_t mode)
{
    if (!name)
        return NULL;

    return create_shm(consumers, producers, name, mode);
}


ri_shm_t* ri_shm_map(int fd)
{
    ri_shm_t *shm = calloc(1, sizeof(ri_shm_t));

    if (!shm)
        goto fail_alloc;

    shm->sys = ri_sys_map(fd);

    if (!shm->sys)
        goto fail_sys;

    int r = client_read_shm(shm);

    if (r < 0)
        goto fail_init;

    return shm;

fail_init:
    ri_sys_delete(shm->sys);
fail_sys:
    free(shm);
fail_alloc:
    return NULL;
}


ri_shm_t* ri_named_shm_map(const char *name)
{
    ri_shm_t *shm = calloc(1, sizeof(ri_shm_t));

    if (!shm)
        goto fail_alloc;

    shm->sys = ri_sys_map_named(name);

    if (!shm->sys)
        goto fail_sys;

    int r = client_read_shm(shm);

    if (r < 0)
        goto fail_init;

    return shm;

fail_init:
    ri_sys_delete(shm->sys);
fail_sys:
    free(shm);
fail_alloc:
    return NULL;
}


void ri_shm_delete(ri_shm_t *shm)
{
    delete_channels(shm);
    ri_sys_delete(shm->sys);
    free(shm);
}


ri_consumer_t* ri_shm_get_consumer(const ri_shm_t *shm, unsigned idx)
{
    if (idx >= shm->layout.consumers.num)
        return NULL;

    return &shm->consumers[idx];
}


ri_producer_t* ri_shm_get_producer(const ri_shm_t *shm, unsigned idx)
{
    if (idx >= shm->layout.producers.num)
        return NULL;

    return &shm->producers[idx];
}


unsigned ri_shm_get_num_consumers(const ri_shm_t *shm)
{
    return shm->layout.consumers.num;
}

unsigned ri_shm_get_num_producers(const ri_shm_t *shm)
{
    return shm->layout.producers.num;
}


int ri_shm_get_fd(const ri_shm_t *shm)
{
    if (!shm->sys)
        return -1;

    return shm->sys->fd;
}

static void dump_table_entry(const table_entry_t *entry, unsigned i, size_t data_offset, size_t meta_offset, void *ptr)
{
    LOG_INF("\t\tentry[%u]: buffer size=%u, data offset=0x%x (%p) meta offset=0x%x (%p) meta size=0x%x",
            i, entry->buffer_size, entry->data_offset, mem_offset(ptr, data_offset + entry->data_offset),
            entry->meta_offset, mem_offset(ptr, meta_offset + entry->meta_offset), entry->meta_size);
}

static void dump_channel(const ri_channel_t *channel, unsigned i)
{
    LOG_INF("\t\t\tchannel[%u]: buf[0]=%p buf[1]=%p buf[2]=%p meta=%p meta size=%zu",
            i, channel->bufs[0], channel->bufs[1], channel->bufs[2], channel->meta.ptr, channel->meta.size);
}


static void dump_group(const shm_group_t *group, void *ptr)
{
    LOG_INF("\t\t table: offset=0x%zx, size=%zu", group->table.offset, group->table.size);
    LOG_INF("\t\t data: offset=0x%zx, size=%zu", group->data.offset, group->data.size);
    LOG_INF("\t\t meta: offset=0x%zx, size=%zu", group->meta.offset, group->meta.size);

    size_t offset = group->table.offset;


    for (unsigned i = 0; i < group->num; i++) {
        table_entry_t *entry = mem_offset(ptr, offset);
        dump_table_entry(entry, i, group->data.offset, group->meta.offset, ptr);
        offset += table_entry_size();
    }
}


void ri_shm_dump(const ri_shm_t *shm)
{
    const shm_layout_t *layout = &shm->layout;

    LOG_INF("shared memory: ptr=%p size=%zu", shm->sys->ptr, shm->sys->size);

    LOG_INF("\tnumber of consumers: %u", layout->consumers.num);
    LOG_INF("\tnumber of producers: %u", layout->producers.num);

    LOG_INF("\n\tconsumers: %u", layout->consumers.num);
    dump_group(&layout->consumers, shm->sys->ptr);

    for (unsigned i = 0; i < layout->consumers.num; i++)
        dump_channel(&shm->consumers[i].channel, i);

    LOG_INF("\n\tproducers: %u", layout->producers.num);
    dump_group(&layout->producers, shm->sys->ptr);

    for (unsigned i = 0; i < layout->producers.num; i++)
        dump_channel(&shm->producers[i].channel, i);

}
