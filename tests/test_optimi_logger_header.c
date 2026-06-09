#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "optimi_logger.h"

typedef struct {
    int called;
    optimi_logger_t* logger;
    optimi_log_level_t level;
    const char* file;
    const char* function;
    int line;
    char message[256];
} log_capture_t;

static log_capture_t g_capture;

static int g_checks_failed;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "    check failed: %s (line %d)\n", #cond, __LINE__); \
        g_checks_failed++; \
    } \
} while (0)

static void reset_capture(void) {
    memset(&g_capture, 0, sizeof(g_capture));
}

optimi_status_t optimi_logger_log(
    optimi_logger_t* logger,
    optimi_log_level_t level,
    const char* file,
    const char* function,
    int line,
    const char* format,
    ...
) {
    va_list args;

    g_capture.called = 1;
    g_capture.logger = logger;
    g_capture.level = level;
    g_capture.file = file;
    g_capture.function = function;
    g_capture.line = line;

    va_start(args, format);
    vsnprintf(g_capture.message, sizeof(g_capture.message), format, args);
    va_end(args);

    return OPTIMI_STATUS_OK;
}

/**
 * Test the values of the status enum.
 */
static void test_status_enum_values(void) {
    CHECK(OPTIMI_STATUS_OK == 0);
    CHECK(OPTIMI_STATUS_INVALID_ARGUMENT == 1);
    CHECK(OPTIMI_STATUS_NOT_INITIALIZED == 2);
    CHECK(OPTIMI_STATUS_IO_ERROR == 3);
    CHECK(OPTIMI_STATUS_QUEUE_FULL == 4);
    CHECK(OPTIMI_STATUS_ALLOCATION_FAILED == 5);
    CHECK(OPTIMI_STATUS_INTERNAL_ERROR == 6);
}

/**
 * Test the values of the log level enum.
 */
static void test_log_level_enum_values(void) {
    CHECK(OPTIMI_LOG_LEVEL_DEBUG == 0);
    CHECK(OPTIMI_LOG_LEVEL_INFO == 1);
    CHECK(OPTIMI_LOG_LEVEL_WARN == 2);
    CHECK(OPTIMI_LOG_LEVEL_ERROR == 3);
    CHECK(OPTIMI_LOG_LEVEL_FATAL == 4);
}

/**
 * Test that the logging macros correctly forward the log level and caller metadata to optimi_logger_log().
 */
static void test_macro_forwards_level_and_metadata(void) {
    optimi_logger_t* fake_logger = (optimi_logger_t*)0x1;
    int expected_line;
    optimi_status_t status;

    reset_capture();
    expected_line = __LINE__ + 1;
    status = OPTIMI_LOG_ERROR(fake_logger, "count=%d", 7);

    CHECK(status == OPTIMI_STATUS_OK);
    CHECK(g_capture.called == 1);
    CHECK(g_capture.logger == fake_logger);
    CHECK(g_capture.level == OPTIMI_LOG_LEVEL_ERROR);
    CHECK(g_capture.line == expected_line);
    CHECK(g_capture.file != NULL);
    CHECK(strstr(g_capture.file, "test_optimi_logger_header.c") != NULL);
    CHECK(g_capture.function != NULL);
    CHECK(strcmp(g_capture.function, __func__) == 0);
    CHECK(strcmp(g_capture.message, "count=7") == 0);
}

/**
 * Test that the logging macros correctly handle the case where no additional arguments are provided.
 */
static void test_macro_without_varargs(void) {
    optimi_logger_t* fake_logger = (optimi_logger_t*)0x2;
    optimi_status_t status;

    reset_capture();
    status = OPTIMI_LOG_INFO(fake_logger, "ready");

    CHECK(status == OPTIMI_STATUS_OK);
    CHECK(g_capture.called == 1);
    CHECK(g_capture.level == OPTIMI_LOG_LEVEL_INFO);
    CHECK(strcmp(g_capture.message, "ready") == 0);
}

/**
 * Test that the warning macro uses the correct log level.
 */
static void test_warning_macro_uses_warn_level(void) {
    reset_capture();
    (void)OPTIMI_LOG_WARNING((optimi_logger_t*)0x3, "warn");

    CHECK(g_capture.called == 1);
    CHECK(g_capture.level == OPTIMI_LOG_LEVEL_WARN);
}

// Test runner
typedef void (*test_fn_t)(void);

typedef struct {
    const char* name;
    test_fn_t fn;
} test_case_t;

static int run_test(const test_case_t* test_case) {
    int failed_before = g_checks_failed;

    test_case->fn();
    if (g_checks_failed == failed_before) {
        printf("[PASS] %s\n", test_case->name);
        return 0;
    }

    printf("[FAIL] %s\n", test_case->name);
    return 1;
}

int main(void) {
    size_t i;
    int failed_tests = 0;
    test_case_t tests[] = {
        {"Status enum values", test_status_enum_values},
        {"Log level enum values", test_log_level_enum_values},
        {"Macro forwards metadata", test_macro_forwards_level_and_metadata},
        {"Macro supports empty varargs", test_macro_without_varargs},
        {"Warning macro uses warn level", test_warning_macro_uses_warn_level}
    };

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        failed_tests += run_test(&tests[i]);
    }

    printf("\nSummary: %zu run, %d failed\n", sizeof(tests) / sizeof(tests[0]), failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
