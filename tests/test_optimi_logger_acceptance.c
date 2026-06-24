#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "optimi_logger.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "    check failed: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while (0)

static int create_temp_dir(char* out_path, size_t out_path_size)
{
    char template_path[] = "/tmp/optimi_logger_accept_XXXXXX";
    char* created = mkdtemp(template_path);

    if (created == NULL) {
        return -1;
    }

    if (strlen(created) + 1U > out_path_size) {
        return -1;
    }

    strcpy(out_path, created);
    return 0;
}

static int remove_directory_files(const char* dir_path)
{
    DIR* dir;
    struct dirent* entry;
    char file_path[PATH_MAX];

    dir = opendir(dir_path);
    if (dir == NULL) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
        if (remove(file_path) != 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    return rmdir(dir_path);
}

static int count_log_files(const char* dir_path)
{
    DIR* dir;
    struct dirent* entry;
    int count = 0;

    dir = opendir(dir_path);
    if (dir == NULL) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (len >= 4U && strcmp(entry->d_name + (len - 4U), ".log") == 0) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

static int find_log_file(const char* dir_path, const char* prefix, char* out_path, size_t out_path_size)
{
    DIR* dir;
    struct dirent* entry;
    size_t prefix_len = strlen(prefix);

    dir = opendir(dir_path);
    if (dir == NULL) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        size_t name_len;

        if (entry->d_name[0] == '.') {
            continue;
        }

        name_len = strlen(entry->d_name);
        if (name_len < prefix_len + 4U) {
            continue;
        }

        if (strncmp(entry->d_name, prefix, prefix_len) != 0) {
            continue;
        }

        if (strcmp(entry->d_name + (name_len - 4U), ".log") != 0) {
            continue;
        }

        snprintf(out_path, out_path_size, "%s/%s", dir_path, entry->d_name);
        closedir(dir);
        return 0;
    }

    closedir(dir);
    return -1;
}

static int file_contains_text(const char* file_path, const char* needle)
{
    FILE* file;
    char buffer[8192];
    size_t bytes;

    file = fopen(file_path, "r");
    if (file == NULL) {
        return 0;
    }

    bytes = fread(buffer, 1U, sizeof(buffer) - 1U, file);
    buffer[bytes] = '\0';
    fclose(file);

    return strstr(buffer, needle) != NULL;
}

static optimi_status_t logv_forward(
    optimi_logger_t* logger,
    optimi_log_level_t level,
    const char* file,
    const char* function,
    int line,
    const char* format,
    ...
)
{
    optimi_status_t status;
    va_list args;

    va_start(args, format);
    status = optimi_logger_logv(logger, level, file, function, line, format, args);
    va_end(args);

    return status;
}

static int test_create_argument_contract(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;

    optimi_logger_config_init_default(&config);
    CHECK(optimi_logger_create(NULL, &config) == OPTIMI_STATUS_INVALID_ARGUMENT);
    CHECK(optimi_logger_create(&logger, NULL) == OPTIMI_STATUS_INVALID_ARGUMENT);

    return 0;
}

static int test_destroy_null_is_safe(void)
{
    optimi_logger_destroy(NULL);
    return 0;
}

static int test_lifecycle_post_shutdown_contract(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_shutdown(logger, 10, OPTIMI_SHUTDOWN_FLUSH_QUEUE) == OPTIMI_STATUS_OK);

    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "post") == OPTIMI_STATUS_NOT_INITIALIZED);
    CHECK(optimi_logger_flush(logger) == OPTIMI_STATUS_NOT_INITIALIZED);
    CHECK(optimi_logger_set_min_level(logger, OPTIMI_LOG_LEVEL_ERROR) == OPTIMI_STATUS_NOT_INITIALIZED);

    optimi_logger_destroy(logger);
    return 0;
}

static int test_min_level_filtering(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;
    char dir_path[PATH_MAX];
    char file_path[PATH_MAX];

    CHECK(create_temp_dir(dir_path, sizeof(dir_path)) == 0);

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 1;
    config.output_path = dir_path;
    config.filename_prefix = "accept_min_level";
    config.min_level = OPTIMI_LOG_LEVEL_INFO;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, "debug-hidden") == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "info-visible") == OPTIMI_STATUS_OK);

    CHECK(find_log_file(dir_path, config.filename_prefix, file_path, sizeof(file_path)) == 0);
    CHECK(!file_contains_text(file_path, "debug-hidden"));
    CHECK(file_contains_text(file_path, "info-visible"));

    optimi_logger_destroy(logger);
    CHECK(remove_directory_files(dir_path) == 0);
    return 0;
}

static int test_log_and_logv_parity(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;
    char dir_path[PATH_MAX];
    char file_path[PATH_MAX];

    CHECK(create_temp_dir(dir_path, sizeof(dir_path)) == 0);

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 1;
    config.output_path = dir_path;
    config.filename_prefix = "accept_logv";

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);

    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "log-call-%d", 1) == OPTIMI_STATUS_OK);
    CHECK(logv_forward(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "logv-call-%d", 2) == OPTIMI_STATUS_OK);

    CHECK(find_log_file(dir_path, config.filename_prefix, file_path, sizeof(file_path)) == 0);
    CHECK(file_contains_text(file_path, "log-call-1"));
    CHECK(file_contains_text(file_path, "logv-call-2"));

    optimi_logger_destroy(logger);
    CHECK(remove_directory_files(dir_path) == 0);
    return 0;
}

static int test_sink_gating_file_disabled(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;
    char dir_path[PATH_MAX];

    CHECK(create_temp_dir(dir_path, sizeof(dir_path)) == 0);

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;
    config.output_path = dir_path;
    config.filename_prefix = "accept_no_file";

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "no-file") == OPTIMI_STATUS_OK);
    CHECK(count_log_files(dir_path) == 0);

    optimi_logger_destroy(logger);
    CHECK(remove_directory_files(dir_path) == 0);
    return 0;
}

static int test_status_string_mapping(void)
{
    CHECK(strcmp(optimi_logger_status_string(OPTIMI_STATUS_OK), "OPTIMI_STATUS_OK") == 0);
    CHECK(strcmp(optimi_logger_status_string(OPTIMI_STATUS_INVALID_ARGUMENT), "OPTIMI_STATUS_INVALID_ARGUMENT") == 0);
    CHECK(strcmp(optimi_logger_status_string(OPTIMI_STATUS_NOT_INITIALIZED), "OPTIMI_STATUS_NOT_INITIALIZED") == 0);
    CHECK(strcmp(optimi_logger_status_string(OPTIMI_STATUS_IO_ERROR), "OPTIMI_STATUS_IO_ERROR") == 0);
    CHECK(strcmp(optimi_logger_status_string((optimi_status_t)999), "OPTIMI_STATUS_UNKNOWN") == 0);

    return 0;
}

typedef int (*test_fn_t)(void);

typedef struct {
    const char* name;
    test_fn_t fn;
} test_case_t;

int main(void)
{
    size_t i;
    int failed = 0;
    test_case_t tests[] = {
        {"create argument contract", test_create_argument_contract},
        {"destroy(NULL) is safe", test_destroy_null_is_safe},
        {"post-shutdown lifecycle contract", test_lifecycle_post_shutdown_contract},
        {"min level filtering", test_min_level_filtering},
        {"log/logv parity", test_log_and_logv_parity},
        {"file sink gating", test_sink_gating_file_disabled},
        {"status string mapping", test_status_string_mapping}
    };

    printf("Logger acceptance integration tests:\n\n");

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        int rc = tests[i].fn();
        if (rc == 0) {
            printf("[PASS] %s\n", tests[i].name);
        } else {
            printf("[FAIL] %s\n", tests[i].name);
            failed++;
        }
    }

    printf("\nSummary: %zu run, %d failed\n", sizeof(tests) / sizeof(tests[0]), failed);
    return failed == 0 ? 0 : 1;
}
