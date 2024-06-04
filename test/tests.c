#include "tests.h"

int main(void)
{
    Suite *rtipc = rtipc_suite();
    Suite *mapper = mapper_suite();
    SRunner *runner = srunner_create(rtipc);
    srunner_add_suite(runner, mapper);

    srunner_run_all(runner, CK_NORMAL);
    int number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return (number_failed == 0) ? 0 : -1;
}
