#include <munit.h>
extern MunitTest interface_tests[];
int main(int argc, char **argv) {
    MunitSuite suite = { const_cast<char *>("/capture/interface"), interface_tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE };
    return munit_suite_main(&suite, nullptr, argc, argv);
}
