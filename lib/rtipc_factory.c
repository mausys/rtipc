#include "rtipc_private.h"

ri_rtipc_t* ri_rtipc_anon_shm_new(const ri_channel_param_t consumers[],
                                  const ri_channel_param_t producers[],
                                  uint32_t cookie)
{
  size_t size = ri_calc_shm_size(consumers, producers);

  ri_shm_t *shm = ri_shm_anon_new(size);

  if (!shm)
    return NULL;

  ri_rtipc_t *rtipc = ri_rtipc_owner_new(shm, consumers, producers, cookie);

  if (!rtipc) {
    ri_shm_delete(shm);
    return NULL;
  }

  return rtipc;
}

ri_rtipc_t* ri_rtipc_named_shm_new(const ri_channel_param_t consumers[],
                                   const ri_channel_param_t producers[],
                                   const char *name,
                                   mode_t mode,
                                   uint32_t cookie)
{
  size_t size = ri_calc_shm_size(consumers, producers);

  ri_shm_t *shm = ri_shm_named_new(size, name, mode);

  if (!shm)
    return NULL;

  ri_rtipc_t *rtipc = ri_rtipc_owner_new(shm, consumers, producers, cookie);

  if (!rtipc) {
    ri_shm_delete(shm);
    return NULL;
  }

  return rtipc;
}

ri_rtipc_t* ri_rtipc_shm_map(int fd, uint32_t cookie)
{
  ri_shm_t *shm = ri_shm_new(fd);

  if (!shm)
    return NULL;

  ri_rtipc_t *rtipc = ri_rtipc_new(shm, cookie);

  if (!rtipc) {
    ri_shm_delete(shm);
    return NULL;
  }

  return rtipc;
}

ri_rtipc_t* ri_rtipc_named_shm_map(const char *name, uint32_t cookie)
{
  ri_shm_t *shm = ri_shm_named_map(name);

  if (!shm)
    return NULL;

  ri_rtipc_t *rtipc = ri_rtipc_new(shm, cookie);

  if (!rtipc) {
    ri_shm_delete(shm);
    return NULL;
  }

  return rtipc;
}
