#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static int create_temp_dir(char* out_path, size_t out_path_size)
{
    char template_path[] = "/tmp/optimi_logger_flush_XXXXXX";
    char* created;

    created = mkdtemp(template_path);
    if (created == NULL) {
        return -1;
    }

    if (strlen(created) + 1U > out_path_size) {
        return -1;
    }

    strcpy(out_path, created);
    return 0;
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

static long file_size_bytes(const char* file_path)
{
    struct stat st;

    if (stat(file_path, &st) != 0) {
        return -1;
    }

    return (long)st.st_size;
}

static int test_flush_null_returns_invalid_argument(void)
{
    CHECK(optimi_logger_flush(NULL) == OPTIMI_STATUS_INVALID_ARGUMENT);
    return 0;
}

static int test_shutdown_null_returns_invalid_argument(void)
{
    CHECK(optimi_logger_shutdown(NULL, 10, OPTIMI_SHUTDOWN_FLUSH_QUEUE) == OPTIMI_STATUS_INVALID_ARGUMENT);
    return 0;
}

static int test_flush_after_shutdown_returns_not_initialized(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_shutdown(logger, 10, OPTIMI_SHUTDOWN_FLUSH_QUEUE) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_flush(logger) == OPTIMI_STATUS_NOT_INITIALIZED);

    optimi_logger_destroy(logger);
    return 0;
}

static int test_immediate_mode_writes_without_manual_flush(void)
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
    config.filename_prefix = "flush_immediate";
    config.flush_mode = OPTIMI_FLUSH_MODE_IMMEDIATE;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "immediate-visible") == OPTIMI_STATUS_OK);
    CHECK(find_log_file(dir_path, config.filename_prefix, file_path, sizeof(file_path)) == 0);
    CHECK(file_contains_text(file_path, "immediate-visible"));

    optimi_logger_destroy(logger);
    CHECK(remove_directory_files(dir_path) == 0);
    return 0;
}

static int test_balanced_mode_requires_manual_flush_for_persistence(void)
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
    config.filename_prefix = "flush_balanced";
    config.flush_mode = OPTIMI_FLUSH_MODE_BALANCED;
    config.flush_interval_ms = 60000;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "balanced-buffered") == OPTIMI_STATUS_OK);
    CHECK(find_log_file(dir_path, config.filename_prefix, file_path, sizeof(file_path)) == 0);
    CHECK(!file_contains_text(file_path, "balanced-buffered"));

    CHECK(optimi_logger_flush(logger) == OPTIMI_STATUS_OK);
    CHECK(file_contains_text(file_path, "balanced-buffered"));

    optimi_logger_destroy(logger);
    CHECK(remove_directory_files(dir_path) == 0);
    return 0;
}

static int test_manual_flush_is_idempotent_without_new_messages(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;
    char dir_path[PATH_MAX];
    char file_path[PATH_MAX];
    long first_size;
    long second_size;

    CHECK(create_temp_dir(dir_path, sizeof(dir_path)) == 0);

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 1;
    config.output_path = dir_path;
    config.filename_prefix = "flush_idempotent";
    config.flush_mode = OPTIMI_FLUSH_MODE_BALANCED;
    config.flush_interval_ms = 60000;

    CHECK(optimi_logger_create(&logger, &config) == OPTIMI_STATUS_OK);
    CHECK(optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "idempotent") == OPTIMI_STATUS_OK);

    CHECK(optimi_logger_flush(logger) == OPTIMI_STATUS_OK);
    CHECK(find_log_file(dir_path, config.filename_prefix, file_path, sizeof(file_path)) == 0);
    first_size = file_size_bytes(file_path);
    CHECK(first_size >= 0);

    CHECK(optimi_logger_flush(logger) == OPTIMI_STATUS_OK);
    second_size = file_size_bytes(file_path);
    CHECK(second_size == first_size);

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
        {"flush(NULL) returns invalid argument", test_flush_null_returns_invalid_argument},
        {"shutdown(NULL) returns invalid argument", test_shutdown_null_returns_invalid_argument},
        {"flush after shutdown returns not initialized", test_flush_after_shutdown_returns_not_initialized},
        {"immediate mode writes without flush", test_immediate_mode_writes_without_manual_flush},
        {"balanced mode persists on manual flush", test_balanced_mode_requires_manual_flush_for_persistence},
        {"manual flush idempotent", test_manual_flush_is_idempotent_without_new_messages}
    };

    printf("Flushing integration tests:\n\n");

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
