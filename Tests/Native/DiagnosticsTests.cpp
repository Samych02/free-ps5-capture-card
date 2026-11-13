#include "Diagnostics.h"
#include <munit.h>
#include <cstring>
static MunitResult quit_policy(const MunitParameter[],void *) {
    auto transient=capture::classifyQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_DISCONNECTED);
    munit_assert_true(transient.done);munit_assert_true(transient.retry);
    auto auth=capture::classifyQuit(CHIAKI_QUIT_REASON_CTRL_CONNECTION_REFUSED);
    munit_assert_false(auth.retry);munit_assert_int(auth.status.error,==,CAPTURE_ERROR_IDENTITY);
    auto busy=capture::classifyQuit(CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE);
    munit_assert_false(busy.retry);munit_assert_int(busy.status.error,==,CAPTURE_ERROR_IN_USE);
    munit_assert_true(strlen(busy.status.message)>0);
    auto stop=capture::classifyQuit(CHIAKI_QUIT_REASON_STOPPED);
    munit_assert_false(stop.retry);munit_assert_int(stop.status.state,==,CAPTURE_STOPPED);
    for(unsigned i=1;i<=8;++i) {
        unsigned delay=capture::retryDelayMilliseconds(i);
        const unsigned expected[]{1000,2000,4000,8000,15000,15000,15000,15000};
        munit_assert_uint(delay,>=,expected[i-1]);munit_assert_uint(delay,<,expected[i-1]+200);
    }
    munit_assert_uint(capture::nextRetryAttempt(4,30000000),==,1);
    munit_assert_uint(capture::nextRetryAttempt(4,29999999),==,5);
    return MUNIT_OK;
}
int main(int argc,char **argv) {
    MunitTest tests[]{{const_cast<char *>("/quit-policy"),quit_policy,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {nullptr,nullptr,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr}};
    MunitSuite suite{const_cast<char *>("/diagnostics"),tests,nullptr,1,MUNIT_SUITE_OPTION_NONE};return munit_suite_main(&suite,nullptr,argc,argv);
}
