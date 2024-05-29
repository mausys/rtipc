#include <check.h>
#include "rtipc/rtipc.h"


START_TEST(create_consumer)
{
    const ri_channel_req_t chns[] = { { .buffer_size = 111 }, { 0 } };

    ri_shm_t *server_shm = ri_anon_shm_new(chns, NULL);
    ck_assert_ptr_nonnull(server_shm);

    int fd = ri_shm_get_fd(server_shm);
    ck_assert_int_ge(fd, 0);

    ri_shm_t *client_shm = ri_shm_map(fd);
    ck_assert_ptr_nonnull(client_shm);

    ri_consumer_t *server_consumer = ri_shm_get_consumer(server_shm, 0);
    ck_assert_ptr_nonnull(server_consumer);
    size_t size = ri_consumer_get_buffer_size(server_consumer);
    ck_assert_uint_ge(size, chns[0].buffer_size);

    ri_consumer_t *server_consumer_null = ri_shm_get_consumer(server_shm, 1);
    ck_assert_ptr_null(server_consumer_null);

    ri_producer_t *server_producer_null = ri_shm_get_producer(server_shm, 0);
    ck_assert_ptr_null(server_producer_null);

    ri_producer_t *client_producer = ri_shm_get_producer(client_shm, 0);
    ck_assert_ptr_nonnull(client_producer);
    size = ri_producer_get_buffer_size(client_producer);
    ck_assert_uint_ge(size, chns[0].buffer_size);

    ri_producer_t *client_producer_null = ri_shm_get_producer(client_shm, 1);
    ck_assert_ptr_null(client_producer_null);

    ri_shm_delete(server_shm);
    ri_shm_delete(client_shm);
}
END_TEST


START_TEST(create_producer)
{
    const ri_channel_req_t chns[] = { { .buffer_size = 111 }, { 0 } };

    ri_shm_t *server_shm = ri_anon_shm_new(NULL, chns);
    ck_assert_ptr_nonnull(server_shm);

    int fd = ri_shm_get_fd(server_shm);
    ck_assert_int_ge(fd, 0);

    ri_shm_t *client_shm = ri_shm_map(fd);
    ck_assert_ptr_nonnull(client_shm);

    ri_producer_t *server_producer = ri_shm_get_producer(server_shm, 0);
    ck_assert_ptr_nonnull(server_producer);
    size_t size = ri_producer_get_buffer_size(server_producer);
    ck_assert_uint_ge(size, chns[0].buffer_size);

    ri_producer_t *server_producer_null = ri_shm_get_producer(server_shm, 1);
    ck_assert_ptr_null(server_producer_null);

    ri_consumer_t *server_consumer_null = ri_shm_get_consumer(server_shm, 0);
    ck_assert_ptr_null(server_consumer_null);

    ri_consumer_t *client_consumer = ri_shm_get_consumer(client_shm, 0);
    ck_assert_ptr_nonnull(client_consumer);
    size = ri_consumer_get_buffer_size(client_consumer);
    ck_assert_uint_ge(size, chns[0].buffer_size);

    ri_consumer_t *client_consumer_null = ri_shm_get_consumer(client_shm, 1);
    ck_assert_ptr_null(client_consumer_null);


    ri_shm_delete(server_shm);
    ri_shm_delete(client_shm);
}
END_TEST


START_TEST(create_both)
{
    const ri_channel_req_t c2s[] = { { .buffer_size = 111 }, { .buffer_size = 222 }, { 0 } };
    const ri_channel_req_t s2c[] = { { .buffer_size = 123 }, { .buffer_size = 456 }, { .buffer_size = 3 }, { 0 } };

    ri_shm_t *server_shm = ri_anon_shm_new(c2s, s2c);
    ck_assert_ptr_nonnull(server_shm);

    int fd = ri_shm_get_fd(server_shm);
    ck_assert_int_ge(fd, 0);

    ri_shm_t *client_shm = ri_shm_map(fd);
    ck_assert_ptr_nonnull(client_shm);

    ri_consumer_t *server_consumer_0 = ri_shm_get_consumer(server_shm, 0);
    ck_assert_ptr_nonnull(server_consumer_0);
    size_t size = ri_consumer_get_buffer_size(server_consumer_0);
    ck_assert_uint_ge(size, c2s[0].buffer_size);

    ri_consumer_t *server_consumer_1 = ri_shm_get_consumer(server_shm, 1);
    ck_assert_ptr_nonnull(server_consumer_1);
    size = ri_consumer_get_buffer_size(server_consumer_1);
    ck_assert_uint_ge(size, c2s[1].buffer_size);

    ri_consumer_t *server_consumer_null = ri_shm_get_consumer(server_shm, 2);
    ck_assert_ptr_null(server_consumer_null);

    ri_producer_t *server_producer_0 = ri_shm_get_producer(server_shm, 0);
    ck_assert_ptr_nonnull(server_producer_0);
    size = ri_producer_get_buffer_size(server_producer_0);
    ck_assert_uint_ge(size, s2c[0].buffer_size);

    ri_producer_t *server_producer_1 = ri_shm_get_producer(server_shm, 1);
    ck_assert_ptr_nonnull(server_producer_1);
    size = ri_producer_get_buffer_size(server_producer_1);
    ck_assert_uint_ge(size, s2c[1].buffer_size);

    ri_producer_t *server_producer_2 = ri_shm_get_producer(server_shm, 2);
    ck_assert_ptr_nonnull(server_producer_2);
    size = ri_producer_get_buffer_size(server_producer_2);
    ck_assert_uint_ge(size, s2c[2].buffer_size);

    ri_producer_t *server_producer_null = ri_shm_get_producer(server_shm, 3);
    ck_assert_ptr_null(server_producer_null);


    ri_consumer_t *client_consumer_0 = ri_shm_get_consumer(client_shm, 0);
    ck_assert_ptr_nonnull(client_consumer_0);
    size = ri_consumer_get_buffer_size(client_consumer_0);
    ck_assert_uint_ge(size, s2c[0].buffer_size);

    ri_consumer_t *client_consumer_1 = ri_shm_get_consumer(client_shm, 1);
    ck_assert_ptr_nonnull(client_consumer_1);
    size = ri_consumer_get_buffer_size(client_consumer_1);
    ck_assert_uint_ge(size, s2c[1].buffer_size);

    ri_consumer_t *client_consumer_2 = ri_shm_get_consumer(client_shm, 2);
    ck_assert_ptr_nonnull(client_consumer_2);
    size = ri_consumer_get_buffer_size(client_consumer_2);
    ck_assert_uint_ge(size, s2c[2].buffer_size);

    ri_consumer_t *client_consumer_null = ri_shm_get_consumer(server_shm, 3);
    ck_assert_ptr_null(client_consumer_null);


    ri_producer_t *client_producer_0 = ri_shm_get_producer(client_shm, 0);
    ck_assert_ptr_nonnull(client_producer_0);
    size = ri_producer_get_buffer_size(client_producer_0);
    ck_assert_uint_ge(size, c2s[0].buffer_size);

    ri_producer_t *client_producer_1 = ri_shm_get_producer(client_shm, 1);
    ck_assert_ptr_nonnull(client_producer_1);
    size = ri_producer_get_buffer_size(client_producer_1);
    ck_assert_uint_ge(size, c2s[1].buffer_size);

    ri_producer_t *client_producer_null = ri_shm_get_producer(client_shm, 2);
    ck_assert_ptr_null(client_producer_null);

    ri_shm_delete(server_shm);
    ri_shm_delete(client_shm);
}
END_TEST


START_TEST(transfer_simple)
{
    const ri_channel_req_t req[] = { { .buffer_size = 123 }, { 0 } };

    ri_shm_t *server_shm = ri_anon_shm_new(req, NULL);
    ck_assert_ptr_nonnull(server_shm);

    int fd = ri_shm_get_fd(server_shm);

    ri_shm_t *client_shm = ri_shm_map(fd);
    ck_assert_ptr_nonnull(client_shm);

    ri_consumer_t *server_consumer = ri_shm_get_consumer(server_shm, 0);
    ck_assert_ptr_nonnull(server_consumer);

    ri_producer_t *client_producer = ri_shm_get_producer(client_shm, 0);
    ck_assert_ptr_nonnull(client_producer);

    void *server_consumer_buffer_0 = ri_consumer_fetch(server_consumer);
    ck_assert_ptr_null(server_consumer_buffer_0);

    void *client_producer_buffer_0 = ri_producer_swap(client_producer);
    ck_assert_ptr_nonnull(client_producer_buffer_0);

    // must be still null, because client didn't send any data yet
    server_consumer_buffer_0 = ri_consumer_fetch(server_consumer);
    ck_assert_ptr_null(server_consumer_buffer_0);

    memset(client_producer_buffer_0, 0xaa, req[0].buffer_size);

    void *client_producer_buffer_1 = ri_producer_swap(client_producer);
    ck_assert_ptr_nonnull(client_producer_buffer_1);
    ck_assert_pstr_ne(client_producer_buffer_0, client_producer_buffer_1);

    memset(client_producer_buffer_1, 0x33, req[0].buffer_size);

    server_consumer_buffer_0 = ri_consumer_fetch(server_consumer);
    ck_assert_ptr_nonnull(server_consumer_buffer_0);

    ck_assert_mem_eq(server_consumer_buffer_0, client_producer_buffer_0, req[0].buffer_size);

    void *server_consumer_buffer_1 = ri_consumer_fetch(server_consumer);
    ck_assert_ptr_eq(server_consumer_buffer_0, server_consumer_buffer_1);

    ri_shm_delete(server_shm);
    ri_shm_delete(client_shm);
}
END_TEST



Suite* rtipc_suite(void)
{
    Suite *s = suite_create("RTIPC");

    /* Core test case */
    TCase *tc_single = tcase_create("Single");
    tcase_add_test(tc_single, create_consumer);
    tcase_add_test(tc_single, create_producer);
    tcase_add_test(tc_single, create_both);

    TCase *tc_multi = tcase_create("Transfer");
    tcase_add_test(tc_multi, transfer_simple);

    suite_add_tcase(s, tc_single);
    suite_add_tcase(s, tc_multi);
    return s;
}


int main(void)
{
    Suite *s = rtipc_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}
