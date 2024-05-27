#include <check.h>
#include "rtipc/rtipc.h"

static void setup(void)
{
}

static void teardown(void)
{
}

START_TEST(test_single_consumer)
{
    const size_t buffer_size = 111;
    ri_channel_req_t chns[] = { { .buffer_size = buffer_size }, { 0 } };

    ri_shm_t *server_shm = ri_anon_shm_new(chns, NULL);
    ck_assert_ptr_nonnull(server_shm);

    int fd = ri_shm_get_fd(server_shm);
    ck_assert_int_ge(fd, 0);

    ri_shm_t *client_shm = ri_shm_map(fd);
    ck_assert_ptr_nonnull(client_shm);

    ri_consumer_t *server_consumer = ri_shm_get_consumer(server_shm, 0);
    ck_assert_ptr_nonnull(server_consumer);
    size_t size = ri_consumer_get_buffer_size(server_consumer);
    ck_assert_uint_ge(size, buffer_size);

    ri_consumer_t *server_consumer_null = ri_shm_get_consumer(server_shm, 1);
    ck_assert_ptr_null(server_consumer_null);
    ri_producer_t *server_producer_null = ri_shm_get_producer(server_shm, 0);
    ck_assert_ptr_null(server_producer_null);

    ri_producer_t *client_producer = ri_shm_get_producer(client_shm, 0);
    ck_assert_ptr_nonnull(client_producer);
    size = ri_producer_get_buffer_size(client_producer);
    ck_assert_uint_ge(size, buffer_size);
    ri_producer_t *client_roducer_null = ri_shm_get_producer(client_shm, 1);
    ck_assert_ptr_null(client_roducer_null);

    void *server_buffer_consumer = ri_consumer_fetch(server_consumer);
    ck_assert_ptr_null(server_buffer_consumer);

    void *client_buffer_producer = ri_producer_swap(client_producer);
    ck_assert_ptr_nonnull(client_buffer_producer);

    server_buffer_consumer = ri_consumer_fetch(server_consumer);
    ck_assert_ptr_null(server_buffer_consumer);

    memset(client_buffer_producer, 0xaa, buffer_size);

    void *client_buffer_producer_next = ri_producer_swap(client_producer);
    ck_assert_ptr_nonnull(client_buffer_producer_next);
    ck_assert_pstr_ne(client_buffer_producer, client_buffer_producer_next);

    memset(client_buffer_producer_next, 0x33, buffer_size);

    server_buffer_consumer = ri_consumer_fetch(server_consumer);
    ck_assert_ptr_nonnull(server_buffer_consumer);

    ck_assert_mem_eq(server_buffer_consumer, client_buffer_producer, buffer_size);

    void *server_buffer_consumer_next = ri_consumer_fetch(server_consumer);
    ck_assert_ptr_eq(server_buffer_consumer, server_buffer_consumer_next);


    ri_shm_delete(server_shm);
    ri_shm_delete(client_shm);
}
END_TEST

START_TEST(test_single_producer)
{
}
END_TEST


START_TEST(test_multi_consumer)
{
}
END_TEST

START_TEST(test_multi_producer)
{
}
END_TEST


START_TEST(test_bidirectional)
{
}
END_TEST




Suite* rtipc_suite(void)
{
    Suite *s = suite_create("RTIPC");

    /* Core test case */
    TCase *tc_single = tcase_create("Single");
    tcase_add_checked_fixture(tc_single, setup, teardown);
    tcase_add_test(tc_single, test_single_consumer);
    tcase_add_test(tc_single, test_single_producer);

    TCase *tc_multi = tcase_create("Multi");
    tcase_add_test(tc_multi, test_multi_consumer);
    tcase_add_test(tc_multi, test_multi_producer);
    tcase_add_test(tc_multi, test_bidirectional);


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
