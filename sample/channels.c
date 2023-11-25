#include "channels.h"
#include <memory.h>

void channel_cmd_to_objects(channel_cmd_t *channel, ri_object_t  *objects)
{
    ri_object_t tmp[] =  {
        RI_OBJECT(channel->header),
        RI_OBJECT(channel->cmd),
        RI_OBJECT(channel->arg1),
        RI_OBJECT(channel->arg2),
        RI_OBJECT(channel->arg3),
        RI_OBJECT_END,
    };

    for (unsigned i = 0; i < sizeof(tmp) / sizeof(tmp[0]); i++)
        tmp[i].meta.id = i;

    memcpy(objects, tmp, sizeof(tmp));
}


void channel_rsp_to_objects(channel_rsp_t *channel, ri_object_t  *objects)
{
    ri_object_t tmp[] =  {
        RI_OBJECT(channel->header),
        RI_OBJECT(channel->cmd),
        RI_OBJECT(channel->data1),
        RI_OBJECT(channel->data2),
        RI_OBJECT(channel->data3),
        RI_OBJECT_END,
    };

    for (unsigned i = 0; i < sizeof(tmp) / sizeof(tmp[0]); i++)
        tmp[i].meta.id = i;

    memcpy(objects, tmp, sizeof(tmp));
}

void channel_rpdo1_to_objects(channel_rpdo1_t *channel, ri_object_t  *objects)
{
    ri_object_t tmp[] =  {
        RI_OBJECT(channel->header),
        RI_OBJECT(channel->data1),
        RI_OBJECT(channel->data2),
        RI_OBJECT_ARRAY(channel->data3, 10000),
        RI_OBJECT(channel->data4),
        RI_OBJECT_END,
    };

    for (unsigned i = 0; i < sizeof(tmp) / sizeof(tmp[0]); i++)
        tmp[i].meta.id = i;

    memcpy(objects, tmp, sizeof(tmp));
}


void channel_rpdo2_to_objects(channel_rpdo2_t *channel, ri_object_t  *objects)
{
    ri_object_t tmp[] =  {
        RI_OBJECT(channel->header),
        RI_OBJECT(channel->data1),
        RI_OBJECT(channel->data2),
        RI_OBJECT(channel->data3),
        RI_OBJECT_END,
    };

    for (unsigned i = 0; i < sizeof(tmp) / sizeof(tmp[0]); i++)
        tmp[i].meta.id = i;

    memcpy(objects, tmp, sizeof(tmp));
}


void channel_tpdo1_to_objects(channel_tpdo1_t *channel, ri_object_t *objects)
{
    ri_object_t tmp[] =  {
        RI_OBJECT(channel->header),
        RI_OBJECT(channel->data1),
        RI_OBJECT(channel->data2),
        RI_OBJECT(channel->data3),
        RI_OBJECT_END,
    };

    for (unsigned i = 0; i < sizeof(tmp) / sizeof(tmp[0]); i++)
        tmp[i].meta.id = i;

    memcpy(objects, tmp, sizeof(tmp));
}
