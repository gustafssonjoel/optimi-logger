#include "optimi_logger.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

typedef struct {
    optimi_log_level_t level;
    char* text;
} optimi_log_entry_t;

struct optimi_logger {
    optimi_logger_config_t config;
    int is_shutdown;
    FILE* file_handle;
    optimi_log_entry_t* queue;
    size_t queue_capacity;
    size_t queue_count;
    size_t queue_head;
    size_t queue_tail;
    uint64_t last_flush_ms;
    atomic_flag lock;
};

static int is_valid_level(optimi_log_level_t level)
{
    return level >= OPTIMI_LOG_LEVEL_DEBUG && level <= OPTIMI_LOG_LEVEL_FATAL;
}

static int is_valid_overflow_policy(optimi_queue_overflow_policy_t policy)
{
    return policy >= OPTIMI_OVERFLOW_ERROR && policy <= OPTIMI_OVERFLOW_DROP_OLDEST;
}

static int is_valid_flush_mode(optimi_flush_mode_t mode)
{
    return mode == OPTIMI_FLUSH_MODE_IMMEDIATE || mode == OPTIMI_FLUSH_MODE_BALANCED;
}

static int is_valid_shutdown_mode(optimi_shutdown_mode_t mode)
{
    return mode >= OPTIMI_SHUTDOWN_FLUSH_QUEUE && mode <= OPTIMI_SHUTDOWN_DROP_QUEUE;
}

static const char* level_string(optimi_log_level_t level)
{
    switch (level) {
        case OPTIMI_LOG_LEVEL_DEBUG:
            return "DEBUG";
        case OPTIMI_LOG_LEVEL_INFO:
            return "INFO";
        case OPTIMI_LOG_LEVEL_WARN:
            return "WARN";
        case OPTIMI_LOG_LEVEL_ERROR:
            return "ERROR";
        case OPTIMI_LOG_LEVEL_FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

static const char* level_color(optimi_log_level_t level)
{
    switch (level) {
        case OPTIMI_LOG_LEVEL_DEBUG:
            return "\033[36m";
        case OPTIMI_LOG_LEVEL_INFO:
            return "\033[32m";
        case OPTIMI_LOG_LEVEL_WARN:
            return "\033[33m";
        case OPTIMI_LOG_LEVEL_ERROR:
            return "\033[31m";
        case OPTIMI_LOG_LEVEL_FATAL:
            return "\033[35m";
        default:
            return "\033[0m";
    }
}

static void logger_lock(optimi_logger_t* logger)
{
    while (atomic_flag_test_and_set_explicit(&logger->lock, memory_order_acquire)) {
    }
}

static void logger_unlock(optimi_logger_t* logger)
{
    atomic_flag_clear_explicit(&logger->lock, memory_order_release);
}

static uint64_t monotonic_ms(void)
{
    struct timespec ts;
#ifdef CLOCK_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
#endif
#ifdef CLOCK_REALTIME
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
#endif
    return (uint64_t)time(NULL) * 1000ULL;
}

static void format_time_now(char* buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm tm_now;

    if (buffer_size == 0) {
        return;
    }

    if (now == (time_t)-1) {
        buffer[0] = '\0';
        return;
    }

    if (localtime_r(&now, &tm_now) == NULL) {
        buffer[0] = '\0';
        return;
    }

    if (strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &tm_now) == 0) {
        buffer[0] = '\0';
    }
}

static int ensure_directory_recursive(const char* path)
{
    char* mutable_path;
    char* cursor;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    mutable_path = malloc(strlen(path) + 1);
    if (mutable_path == NULL) {
        return -1;
    }

    strcpy(mutable_path, path);

    for (cursor = mutable_path + 1; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (mutable_path[0] != '\0' && mkdir(mutable_path, 0775) != 0 && errno != EEXIST) {
                free(mutable_path);
                return -1;
            }
            *cursor = '/';
        }
    }

    if (mkdir(mutable_path, 0775) != 0 && errno != EEXIST) {
        free(mutable_path);
        return -1;
    }

    free(mutable_path);
    return 0;
}

static char* build_log_file_path(const optimi_logger_config_t* config)
{
    char date_part[16] = {0};
    time_t now;
    struct tm tm_now;
    size_t base_len;
    size_t prefix_len;
    size_t total_len;
    char* path;
    int has_trailing_slash;

    if (config == NULL || config->output_path == NULL || config->filename_prefix == NULL) {
        return NULL;
    }

    if (config->timestamp_in_filename) {
        now = time(NULL);
        if (now == (time_t)-1 || localtime_r(&now, &tm_now) == NULL ||
            strftime(date_part, sizeof(date_part), "%Y%m%d", &tm_now) == 0) {
            return NULL;
        }
    }

    base_len = strlen(config->output_path);
    prefix_len = strlen(config->filename_prefix);
    has_trailing_slash = (base_len > 0 && config->output_path[base_len - 1] == '/');

    total_len = base_len + (has_trailing_slash ? 0U : 1U) + prefix_len +
                (config->timestamp_in_filename ? (1U + strlen(date_part)) : 0U) +
                strlen(".log") + 1U;

    path = malloc(total_len);
    if (path == NULL) {
        return NULL;
    }

    if (config->timestamp_in_filename) {
        snprintf(
            path,
            total_len,
            "%s%s%s_%s.log",
            config->output_path,
            has_trailing_slash ? "" : "/",
            config->filename_prefix,
            date_part
        );
    } else {
        snprintf(
            path,
            total_len,
            "%s%s%s.log",
            config->output_path,
            has_trailing_slash ? "" : "/",
            config->filename_prefix
        );
    }

    return path;
}

static void clear_queue_locked(optimi_logger_t* logger)
{
    size_t i;

    for (i = 0; i < logger->queue_capacity; ++i) {
        free(logger->queue[i].text);
        logger->queue[i].text = NULL;
    }

    logger->queue_count = 0;
    logger->queue_head = 0;
    logger->queue_tail = 0;
}

static optimi_status_t write_entry_locked(optimi_logger_t* logger, const optimi_log_entry_t* entry)
{
    if (logger->config.enable_console) {
        if (logger->config.enable_colors) {
            if (fprintf(stdout, "%s%s\033[0m", level_color(entry->level), entry->text) < 0) {
                return OPTIMI_STATUS_IO_ERROR;
            }
        } else {
            if (fputs(entry->text, stdout) == EOF) {
                return OPTIMI_STATUS_IO_ERROR;
            }
        }
    }

    if (logger->config.enable_file && logger->file_handle != NULL) {
        if (fputs(entry->text, logger->file_handle) == EOF) {
            return OPTIMI_STATUS_IO_ERROR;
        }
    }

    return OPTIMI_STATUS_OK;
}

static optimi_status_t flush_sinks_locked(optimi_logger_t* logger)
{
    if (logger->config.enable_console && fflush(stdout) != 0) {
        return OPTIMI_STATUS_IO_ERROR;
    }

    if (logger->config.enable_file && logger->file_handle != NULL && fflush(logger->file_handle) != 0) {
        return OPTIMI_STATUS_IO_ERROR;
    }

    return OPTIMI_STATUS_OK;
}

static optimi_status_t drain_queue_locked(optimi_logger_t* logger, uint32_t timeout_ms, int best_effort)
{
    optimi_status_t status = OPTIMI_STATUS_OK;
    uint64_t start_ms = monotonic_ms();

    while (logger->queue_count > 0) {
        optimi_log_entry_t* entry;
        optimi_status_t write_status;

        if (best_effort && timeout_ms > 0 && monotonic_ms() - start_ms >= timeout_ms) {
            break;
        }

        entry = &logger->queue[logger->queue_head];
        write_status = write_entry_locked(logger, entry);
        if (write_status != OPTIMI_STATUS_OK && status == OPTIMI_STATUS_OK) {
            status = write_status;
        }

        free(entry->text);
        entry->text = NULL;

        logger->queue_head = (logger->queue_head + 1U) % logger->queue_capacity;
        logger->queue_count--;
    }

    logger->last_flush_ms = monotonic_ms();

    if (flush_sinks_locked(logger) != OPTIMI_STATUS_OK && status == OPTIMI_STATUS_OK) {
        status = OPTIMI_STATUS_IO_ERROR;
    }

    return status;
}

static optimi_status_t enqueue_locked(
    optimi_logger_t* logger,
    optimi_log_level_t level,
    char* text,
    int* out_dropped_newest
)
{
    if (out_dropped_newest != NULL) {
        *out_dropped_newest = 0;
    }

    if (logger->queue_count == logger->queue_capacity) {
        if (logger->config.overflow_policy == OPTIMI_OVERFLOW_ERROR) {
            return OPTIMI_STATUS_QUEUE_FULL;
        }

        if (logger->config.overflow_policy == OPTIMI_OVERFLOW_DROP_NEWEST) {
            if (out_dropped_newest != NULL) {
                *out_dropped_newest = 1;
            }
            return OPTIMI_STATUS_OK;
        }

        if (logger->config.overflow_policy == OPTIMI_OVERFLOW_DROP_OLDEST) {
            optimi_log_entry_t* oldest = &logger->queue[logger->queue_head];
            free(oldest->text);
            oldest->text = NULL;
            logger->queue_head = (logger->queue_head + 1U) % logger->queue_capacity;
            logger->queue_count--;
        }
    }

    logger->queue[logger->queue_tail].level = level;
    logger->queue[logger->queue_tail].text = text;
    logger->queue_tail = (logger->queue_tail + 1U) % logger->queue_capacity;
    logger->queue_count++;

    return OPTIMI_STATUS_OK;
}

static optimi_status_t maybe_auto_flush_locked(optimi_logger_t* logger)
{
    uint64_t now_ms;

    if (logger->config.flush_mode == OPTIMI_FLUSH_MODE_IMMEDIATE) {
        return drain_queue_locked(logger, 0, 0);
    }

    if (logger->config.flush_interval_ms == 0) {
        return OPTIMI_STATUS_OK;
    }

    now_ms = monotonic_ms();
    if (now_ms - logger->last_flush_ms >= logger->config.flush_interval_ms) {
        return drain_queue_locked(logger, 0, 0);
    }

    return OPTIMI_STATUS_OK;
}

static optimi_status_t format_entry(
    optimi_log_level_t level,
    const char* file,
    const char* function,
    int line,
    const char* format,
    va_list args,
    char** out_text
)
{
    va_list copy;
    int msg_size;
    int line_size;
    char* msg;
    char* full_line;
    char time_text[32];
    const char* safe_file = file ? file : "?";
    const char* safe_function = function ? function : "?";

    *out_text = NULL;

    va_copy(copy, args);
    msg_size = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (msg_size < 0) {
        return OPTIMI_STATUS_INTERNAL_ERROR;
    }

    msg = malloc((size_t)msg_size + 1U);
    if (msg == NULL) {
        return OPTIMI_STATUS_ALLOCATION_FAILED;
    }

    if (vsnprintf(msg, (size_t)msg_size + 1U, format, args) < 0) {
        free(msg);
        return OPTIMI_STATUS_INTERNAL_ERROR;
    }

    format_time_now(time_text, sizeof(time_text));

    line_size = snprintf(
        NULL,
        0,
        "[%s] [%s] %s:%d %s: %s\n",
        time_text[0] == '\0' ? "time-unavailable" : time_text,
        level_string(level),
        safe_file,
        line,
        safe_function,
        msg
    );
    if (line_size < 0) {
        free(msg);
        return OPTIMI_STATUS_INTERNAL_ERROR;
    }

    full_line = malloc((size_t)line_size + 1U);
    if (full_line == NULL) {
        free(msg);
        return OPTIMI_STATUS_ALLOCATION_FAILED;
    }

    snprintf(
        full_line,
        (size_t)line_size + 1U,
        "[%s] [%s] %s:%d %s: %s\n",
        time_text[0] == '\0' ? "time-unavailable" : time_text,
        level_string(level),
        safe_file,
        line,
        safe_function,
        msg
    );

    free(msg);
    *out_text = full_line;
    return OPTIMI_STATUS_OK;
}

/*
 * Logger implementation backing the public logger API.
 */

void optimi_logger_config_init_default(optimi_logger_config_t* config)
{
    if (config == NULL) {
        return;
    }

    config->output_path = ".";
    config->filename_prefix = "optimi";
    config->timestamp_in_filename = 0;
    config->min_level = OPTIMI_LOG_LEVEL_INFO;
    config->enable_console = 1;
    config->enable_file = 0;
    config->enable_colors = 0;
    config->queue_capacity = 256;
    config->overflow_policy = OPTIMI_OVERFLOW_ERROR;
    config->flush_mode = OPTIMI_FLUSH_MODE_IMMEDIATE;
    config->flush_interval_ms = 500;
}

optimi_status_t optimi_logger_create(
    optimi_logger_t** out_logger,
    const optimi_logger_config_t* config
)
{
    optimi_logger_t* logger;

    if (out_logger == NULL || config == NULL) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }

    *out_logger = NULL;

    if (!is_valid_level(config->min_level) ||
        !is_valid_overflow_policy(config->overflow_policy) ||
        !is_valid_flush_mode(config->flush_mode) ||
        config->queue_capacity == 0) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }

    if (config->enable_file) {
        char* file_path;

        if (config->output_path == NULL || config->filename_prefix == NULL ||
            config->filename_prefix[0] == '\0') {
            return OPTIMI_STATUS_INVALID_ARGUMENT;
        }

        if (ensure_directory_recursive(config->output_path) != 0) {
            return OPTIMI_STATUS_IO_ERROR;
        }

        file_path = build_log_file_path(config);
        if (file_path == NULL) {
            return OPTIMI_STATUS_ALLOCATION_FAILED;
        }

        logger = calloc(1, sizeof(*logger));
        if (logger == NULL) {
            free(file_path);
            return OPTIMI_STATUS_ALLOCATION_FAILED;
        }

        logger->file_handle = fopen(file_path, "a");
        free(file_path);
        if (logger->file_handle == NULL) {
            free(logger);
            return OPTIMI_STATUS_IO_ERROR;
        }
    } else {
        logger = calloc(1, sizeof(*logger));
        if (logger == NULL) {
            return OPTIMI_STATUS_ALLOCATION_FAILED;
        }
    }

    logger->queue = calloc(config->queue_capacity, sizeof(*logger->queue));
    if (logger->queue == NULL) {
        if (logger->file_handle != NULL) {
            fclose(logger->file_handle);
        }
        free(logger);
        return OPTIMI_STATUS_ALLOCATION_FAILED;
    }

    logger->config = *config;
    logger->queue_capacity = config->queue_capacity;
    logger->queue_count = 0;
    logger->queue_head = 0;
    logger->queue_tail = 0;
    logger->is_shutdown = 0;
    logger->last_flush_ms = monotonic_ms();
    atomic_flag_clear(&logger->lock);

    *out_logger = logger;
    return OPTIMI_STATUS_OK;
}

void optimi_logger_destroy(optimi_logger_t* logger)
{
    if (logger == NULL) {
        return;
    }

    logger_lock(logger);
    clear_queue_locked(logger);
    if (logger->file_handle != NULL) {
        fclose(logger->file_handle);
        logger->file_handle = NULL;
    }
    logger_unlock(logger);

    free(logger->queue);
    free(logger);
}

optimi_status_t optimi_logger_set_min_level(
    optimi_logger_t* logger,
    optimi_log_level_t min_level
)
{
    if (logger == NULL || !is_valid_level(min_level)) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }

    logger_lock(logger);
    if (logger->is_shutdown) {
        logger_unlock(logger);
        return OPTIMI_STATUS_NOT_INITIALIZED;
    }

    logger->config.min_level = min_level;
    logger_unlock(logger);

    return OPTIMI_STATUS_OK;
}

optimi_status_t optimi_logger_flush(optimi_logger_t* logger)
{
    optimi_status_t status;

    if (logger == NULL) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }

    logger_lock(logger);
    if (logger->is_shutdown) {
        logger_unlock(logger);
        return OPTIMI_STATUS_NOT_INITIALIZED;
    }

    status = drain_queue_locked(logger, 0, 0);
    logger_unlock(logger);

    return status;
}

optimi_status_t optimi_logger_shutdown(
    optimi_logger_t* logger,
    uint32_t timeout_ms,
    optimi_shutdown_mode_t mode
)
{
    optimi_status_t status = OPTIMI_STATUS_OK;

    if (logger == NULL || !is_valid_shutdown_mode(mode)) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }

    logger_lock(logger);
    if (logger->is_shutdown) {
        logger_unlock(logger);
        return OPTIMI_STATUS_NOT_INITIALIZED;
    }

    switch (mode) {
        case OPTIMI_SHUTDOWN_FLUSH_QUEUE:
            status = drain_queue_locked(logger, 0, 0);
            break;
        case OPTIMI_SHUTDOWN_BEST_EFFORT:
            status = drain_queue_locked(logger, timeout_ms, 1);
            break;
        case OPTIMI_SHUTDOWN_DROP_QUEUE:
            clear_queue_locked(logger);
            status = flush_sinks_locked(logger);
            break;
        default:
            status = OPTIMI_STATUS_INVALID_ARGUMENT;
            break;
    }

    logger->is_shutdown = 1;
    logger_unlock(logger);

    return status;
}

optimi_status_t optimi_logger_log(
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

    if (logger == NULL || format == NULL || !is_valid_level(level)) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }

    va_start(args, format);
    status = optimi_logger_logv(logger, level, file, function, line, format, args);
    va_end(args);

    return status;
}

optimi_status_t optimi_logger_logv(
    optimi_logger_t* logger,
    optimi_log_level_t level,
    const char* file,
    const char* function,
    int line,
    const char* format,
    va_list args
)
{
    optimi_status_t status;
    char* entry_text = NULL;
    int dropped_newest = 0;

    if (logger == NULL || format == NULL || !is_valid_level(level)) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }

    logger_lock(logger);

    if (logger->is_shutdown) {
        logger_unlock(logger);
        return OPTIMI_STATUS_NOT_INITIALIZED;
    }

    if (level < logger->config.min_level) {
        logger_unlock(logger);
        return OPTIMI_STATUS_OK;
    }

    status = format_entry(level, file, function, line, format, args, &entry_text);
    if (status != OPTIMI_STATUS_OK) {
        logger_unlock(logger);
        return status;
    }

    status = enqueue_locked(logger, level, entry_text, &dropped_newest);
    if (status == OPTIMI_STATUS_QUEUE_FULL) {
        free(entry_text);
        logger_unlock(logger);
        return status;
    }

    if (dropped_newest) {
        free(entry_text);
    }

    if (status == OPTIMI_STATUS_OK) {
        status = maybe_auto_flush_locked(logger);
    }

    logger_unlock(logger);
    return status;
}

const char* optimi_logger_status_string(optimi_status_t status)
{
    switch (status) {
        case OPTIMI_STATUS_OK:
            return "OPTIMI_STATUS_OK";
        case OPTIMI_STATUS_INVALID_ARGUMENT:
            return "OPTIMI_STATUS_INVALID_ARGUMENT";
        case OPTIMI_STATUS_NOT_INITIALIZED:
            return "OPTIMI_STATUS_NOT_INITIALIZED";
        case OPTIMI_STATUS_IO_ERROR:
            return "OPTIMI_STATUS_IO_ERROR";
        case OPTIMI_STATUS_QUEUE_FULL:
            return "OPTIMI_STATUS_QUEUE_FULL";
        case OPTIMI_STATUS_ALLOCATION_FAILED:
            return "OPTIMI_STATUS_ALLOCATION_FAILED";
        case OPTIMI_STATUS_INTERNAL_ERROR:
            return "OPTIMI_STATUS_INTERNAL_ERROR";
        case OPTIMI_STATUS_UNIMPLEMENTED:
            return "OPTIMI_STATUS_UNIMPLEMENTED";
        default:
            return "OPTIMI_STATUS_UNKNOWN";
    }
}
