#include "myobjects.h"



ri_odb_t *myobjects_create_odb(void)
{
    ri_odb_t *odb = ri_odb_new(C2S_CHANNEL_ID_LAST + 1, S2C_CHANNEL_ID_LAST + 1);

    if (!odb)
        return NULL;

    return odb;
}



