#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
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
    char template_path[] = "/tmp/optimi_logger_config_XXXXXX";
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

static int test_invalid_basic_config_is_rejected(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;

    optimi_logger_config_init_default(&config);

    config.queue_capacity = 0;
    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_INVALID_ARGUMENT);

    optimi_logger_config_init_default(&config);
    config.min_level = (optimi_log_level_t)99;
    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_INVALID_ARGUMENT);

    optimi_logger_config_init_default(&config);
    config.overflow_policy = (optimi_queue_overflow_policy_t)99;
    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_INVALID_ARGUMENT);

    optimi_logger_config_init_default(&config);
    config.flush_mode = (optimi_flush_mode_t)99;
    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_INVALID_ARGUMENT);

    return 0;
}

static int test_invalid_file_config_is_rejected(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;

    optimi_logger_config_init_default(&config);
    config.enable_file = 1;
    config.enable_console = 0;
    config.output_path = NULL;
    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_INVALID_ARGUMENT);

    optimi_logger_config_init_default(&config);
    config.enable_file = 1;
    config.enable_console = 0;
    config.filename_prefix = NULL;
    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_INVALID_ARGUMENT);

    optimi_logger_config_init_default(&config);
    config.enable_file = 1;
    config.enable_console = 0;
    config.filename_prefix = "";
    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_INVALID_ARGUMENT);

    return 0;
}

static int test_relative_output_path_creates_file(void)
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
    config.filename_prefix = "cfg_relative";

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "config-path") == OPTIMI_STATUS_OK);
    CHECK(find_log_file(dir_path, "cfg_relative", file_path, sizeof(file_path)) == 0);
    CHECK(file_contains_text(file_path, "config-path"));

    optimi_logger_destroy(logger);
    CHECK(remove_directory_files(dir_path) == 0);
    return 0;
}

static int test_timestamp_filename_behavior(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;
    char dir_path[PATH_MAX];
    char file_path[PATH_MAX];
    const char* base_name;

    CHECK(create_temp_dir(dir_path, sizeof(dir_path)) == 0);

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 1;
    config.output_path = dir_path;
    config.filename_prefix = "cfg_stamp";
    config.timestamp_in_filename = 1;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "stamp") == OPTIMI_STATUS_OK);
    CHECK(find_log_file(dir_path, "cfg_stamp", file_path, sizeof(file_path)) == 0);

    base_name = strrchr(file_path, '/');
    base_name = (base_name == NULL) ? file_path : base_name + 1;
    CHECK(strstr(base_name, "cfg_stamp_") == base_name);

    optimi_logger_destroy(logger);
    CHECK(remove_directory_files(dir_path) == 0);
    return 0;
}

static int test_overflow_error_policy_returns_queue_full(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;
    config.queue_capacity = 1;
    config.overflow_policy = OPTIMI_OVERFLOW_ERROR;
    config.flush_mode = OPTIMI_FLUSH_MODE_BALANCED;
    config.flush_interval_ms = 60000;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "first") == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "second") == OPTIMI_STATUS_QUEUE_FULL);

    optimi_logger_destroy(logger);
    return 0;
}

static int test_overflow_drop_policies_are_applied(void)
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
    config.filename_prefix = "cfg_drop";
    config.queue_capacity = 1;
    config.flush_mode = OPTIMI_FLUSH_MODE_BALANCED;
    config.flush_interval_ms = 60000;
    config.overflow_policy = OPTIMI_OVERFLOW_DROP_NEWEST;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "keep-oldest") == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "drop-newest") == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_flush(logger) == OPTIMI_STATUS_OK);
    CHECK(find_log_file(dir_path, "cfg_drop", file_path, sizeof(file_path)) == 0);
    CHECK(file_contains_text(file_path, "keep-oldest"));
    CHECK(!file_contains_text(file_path, "drop-newest"));
    optimi_logger_destroy(logger);

    CHECK(remove(file_path) == 0);

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 1;
    config.output_path = dir_path;
    config.filename_prefix = "cfg_drop";
    config.queue_capacity = 1;
    config.flush_mode = OPTIMI_FLUSH_MODE_BALANCED;
    config.flush_interval_ms = 60000;
    config.overflow_policy = OPTIMI_OVERFLOW_DROP_OLDEST;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "drop-oldest") == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "keep-newest") == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_flush(logger) == OPTIMI_STATUS_OK);
    CHECK(find_log_file(dir_path, "cfg_drop", file_path, sizeof(file_path)) == 0);
    CHECK(!file_contains_text(file_path, "drop-oldest"));
    CHECK(file_contains_text(file_path, "keep-newest"));

    optimi_logger_destroy(logger);
    CHECK(remove_directory_files(dir_path) == 0);
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
        {"invalid basic config rejected", test_invalid_basic_config_is_rejected},
        {"invalid file config rejected", test_invalid_file_config_is_rejected},
        {"relative output path creates file", test_relative_output_path_creates_file},
        {"timestamp filename behavior", test_timestamp_filename_behavior},
        {"overflow error returns queue full", test_overflow_error_policy_returns_queue_full},
        {"overflow drop policies", test_overflow_drop_policies_are_applied}
    };

    printf("Configuration integration tests:\n\n");

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
