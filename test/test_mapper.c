#include "rtipc/object.h"
#include "rtipc/odb.h"
#include "rtipc/mapper.h"
#include <stdio.h>

#include  "tests.h"


typedef struct {
    uint8_t u8;
    uint16_t arr_u16[17];
    double f64;
    uint32_t u32;
    uint64_t u64;
    float f32;
    uint16_t u16;
} c2s_channel_0_t;


static bool mem_check(const void *p, int c, size_t n)
{
    const uint8_t *a = p;

    for (unsigned i = 0; i < n; i++) {
        if (a[i] != c) {
            return false;
        }
    }

    return true;
}


START_TEST(create_simple_channel)
{
    ri_odb_t *odb = ri_odb_new(1, 0);

    ri_consumer_vector_t *consumer_vector_0 = ri_odb_get_consumer_vector(odb, 0);
    ck_assert_ptr_nonnull(consumer_vector_0);

    ri_consumer_vector_t *consumer_vector_null = ri_odb_get_consumer_vector(odb, 1);
    ck_assert_ptr_null(consumer_vector_null);

    ri_producer_vector_t *producer_vector_null = ri_odb_get_producer_vector(odb, 0);
    ck_assert_ptr_null(producer_vector_null);

    ri_consumer_vector_add(consumer_vector_0, &RI_OBJECT_ID(1, uint8_t));
    ri_consumer_vector_add(consumer_vector_0, &RI_OBJECT_ARRAY_ID(2, uint16_t, 17));
    ri_consumer_vector_add(consumer_vector_0, &RI_OBJECT_ID(3, double));
    ri_consumer_vector_add(consumer_vector_0, &RI_OBJECT_ID(4, uint32_t));
    ri_consumer_vector_add(consumer_vector_0, &RI_OBJECT_ID(5, uint64_t));
    ri_consumer_vector_add(consumer_vector_0, &RI_OBJECT_ID(6, float));
    ri_consumer_vector_add(consumer_vector_0, &RI_OBJECT_ID(7, uint16_t));

    ri_shm_mapper_t *server_shm_mapper = ri_odb_create_anon_shm(odb);

    ri_odb_delete(odb);

    int fd = ri_shm_mapper_get_fd(server_shm_mapper);

    ri_shm_mapper_t *client_shm_mapper = ri_shm_mapper_map(fd);


    ri_consumer_mapper_t *server_consumer_mapper = ri_shm_mapper_get_consumer(server_shm_mapper, 0);
    ck_assert_ptr_nonnull(server_consumer_mapper);

    ri_producer_mapper_t *client_producer_mapper = ri_shm_mapper_get_producer(client_shm_mapper, 0);
    ck_assert_ptr_nonnull(client_producer_mapper);

    uint8_t u8 = 0x11;
    uint16_t arr_u16[17] = {0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206, 0x0207 ,0x0208,
                            0x0209, 0x0210, 0x0211, 0x0212, 0x0213, 0x0214, 0x0215, 0x0216, 0x0217};
    double f64 = 3.0;
    uint32_t u32 = 0x04040404;
    uint64_t u64 = 0x0505050505050505;
    float f32 = 6.0f;
    uint16_t u16 = 0x707;

    ri_producer_object_t *producer_object = ri_producer_mapper_get_object(client_producer_mapper, 0);
    ri_producer_object_copy(producer_object, &u8);

    producer_object = ri_producer_mapper_get_object(client_producer_mapper, 1);
    ri_producer_object_copy(producer_object, arr_u16);

    producer_object = ri_producer_mapper_get_object(client_producer_mapper, 2);
    ri_producer_object_copy(producer_object, &f64);

    producer_object = ri_producer_mapper_get_object(client_producer_mapper, 3);
    ri_producer_object_copy(producer_object, &u32);

    producer_object = ri_producer_mapper_get_object(client_producer_mapper, 4);
    ri_producer_object_copy(producer_object, &u64);

    producer_object = ri_producer_mapper_get_object(client_producer_mapper, 5);
    ri_producer_object_copy(producer_object, &f32);

    producer_object = ri_producer_mapper_get_object(client_producer_mapper, 6);
    ri_producer_object_copy(producer_object, &u16);

     ri_producer_mapper_update(client_producer_mapper);

     ri_consumer_mapper_update(server_consumer_mapper);

     ri_consumer_object_t *consumer_object = ri_consumer_mapper_get_object(server_consumer_mapper, 0);
     const void *ptr = ri_consumer_object_get_pointer(consumer_object);
     uintptr_t start = (uintptr_t)ptr;
     const ri_object_meta_t *meta = ri_consumer_object_get_meta(consumer_object);
     ck_assert_uint_eq(meta->id, 1);
     ck_assert_uint_eq(meta->size, sizeof(u8));
     ck_assert_uint_eq(meta->align, alignof(uint8_t));
     ck_assert_mem_eq(ptr, &u8, sizeof(u8));

     consumer_object = ri_consumer_mapper_get_object(server_consumer_mapper, 1);
     ptr = ri_consumer_object_get_pointer(consumer_object);
     ck_assert_mem_eq(ptr, arr_u16, sizeof(arr_u16));
     uintptr_t offset = (uintptr_t)ptr - start;
     meta = ri_consumer_object_get_meta(consumer_object);
     ck_assert_uint_eq(meta->id, 2);
     ck_assert_uint_eq(meta->size, sizeof(arr_u16));
     ck_assert_uint_eq(meta->align, alignof(uint16_t));
     ck_assert_uint_eq(offset, offsetof(c2s_channel_0_t, arr_u16));

     consumer_object = ri_consumer_mapper_get_object(server_consumer_mapper, 2);
     ptr = ri_consumer_object_get_pointer(consumer_object);
     ck_assert_mem_eq(ptr, &f64, sizeof(f64));
     offset = (uintptr_t)ptr - start;
     meta = ri_consumer_object_get_meta(consumer_object);
     ck_assert_uint_eq(meta->id, 3);
     ck_assert_uint_eq(meta->size, sizeof(f64));
     ck_assert_uint_eq(meta->align, alignof(double));
     ck_assert_uint_eq(offset, offsetof(c2s_channel_0_t, f64));

     consumer_object = ri_consumer_mapper_get_object(server_consumer_mapper, 3);
     ptr = ri_consumer_object_get_pointer(consumer_object);
     ck_assert_mem_eq(ptr, &u32, sizeof(u32));
     offset = (uintptr_t)ptr - start;
     meta = ri_consumer_object_get_meta(consumer_object);
     ck_assert_uint_eq(meta->id, 4);
     ck_assert_uint_eq(meta->size, sizeof(u32));
     ck_assert_uint_eq(meta->align, alignof(uint32_t));
     ck_assert_uint_eq(offset, offsetof(c2s_channel_0_t, u32));

     consumer_object = ri_consumer_mapper_get_object(server_consumer_mapper, 4);
     ptr = ri_consumer_object_get_pointer(consumer_object);
     ck_assert_mem_eq(ptr, &u64, sizeof(u64));
     offset = (uintptr_t)ptr - start;
     meta = ri_consumer_object_get_meta(consumer_object);
     ck_assert_uint_eq(meta->id, 5);
     ck_assert_uint_eq(meta->size, sizeof(u64));
     ck_assert_uint_eq(meta->align, alignof(uint64_t));
     ck_assert_uint_eq(offset, offsetof(c2s_channel_0_t, u64));

     consumer_object = ri_consumer_mapper_get_object(server_consumer_mapper, 5);
     ptr = ri_consumer_object_get_pointer(consumer_object);
     ck_assert_mem_eq(ptr, &f32, sizeof(f32));
     offset = (uintptr_t)ptr - start;
     meta = ri_consumer_object_get_meta(consumer_object);
     ck_assert_uint_eq(meta->id, 6);
     ck_assert_uint_eq(meta->size, sizeof(f32));
     ck_assert_uint_eq(meta->align, alignof(float));
     ck_assert_uint_eq(offset, offsetof(c2s_channel_0_t, f32));

     consumer_object = ri_consumer_mapper_get_object(server_consumer_mapper, 6);
     ptr = ri_consumer_object_get_pointer(consumer_object);
     ck_assert_mem_eq(ptr, &u16, sizeof(u16));
     offset = (uintptr_t)ptr - start;
     meta = ri_consumer_object_get_meta(consumer_object);
     ck_assert_uint_eq(meta->id, 7);
     ck_assert_uint_eq(meta->size, sizeof(u16));
     ck_assert_uint_eq(meta->align, alignof(uint16_t));
     ck_assert_uint_eq(offset, offsetof(c2s_channel_0_t, u16));

    ri_shm_mapper_delete(client_shm_mapper);
    ri_shm_mapper_delete(server_shm_mapper);
}
END_TEST


Suite* mapper_suite(void)
{
    Suite *s = suite_create("Mapper");

    /* Core test case */
    TCase *tc_create = tcase_create("Create");
    tcase_add_test(tc_create, create_simple_channel);

    suite_add_tcase(s, tc_create);
    return s;
}


