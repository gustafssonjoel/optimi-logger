#ifndef OPTIMI_LOGGER_H
#define OPTIMI_LOGGER_H

/**
 * @file optimi_logger.h
 * @brief Public API for the Optimi C logger library.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef struct optimi_logger optimi_logger_t;

/**
 * @brief Status codes returned by logger API functions.
 */
typedef enum {
    /** Operation completed successfully. */
    OPTIMI_STATUS_OK = 0,
    /** One or more arguments were invalid. */
    OPTIMI_STATUS_INVALID_ARGUMENT,
    /** Logger instance is not initialized or already shut down. */
    OPTIMI_STATUS_NOT_INITIALIZED,
    /** A filesystem or I/O operation failed. */
    OPTIMI_STATUS_IO_ERROR,
    /** Queue was full and enqueue failed under current overflow policy. */
    OPTIMI_STATUS_QUEUE_FULL,
    /** Memory allocation failed. */
    OPTIMI_STATUS_ALLOCATION_FAILED,
    /** Internal error not covered by other status codes. */
    OPTIMI_STATUS_INTERNAL_ERROR
} optimi_status_t;

/**
 * @brief Log severity levels.
 */
typedef enum {
    OPTIMI_LOG_LEVEL_DEBUG = 0,
    OPTIMI_LOG_LEVEL_INFO,
    OPTIMI_LOG_LEVEL_WARN,
    OPTIMI_LOG_LEVEL_ERROR,
    OPTIMI_LOG_LEVEL_FATAL
} optimi_log_level_t;

/**
 * @brief Defines the behaviour when the message queue is full
 */
typedef enum {
    /** Return an error when the queue is full. */
    OPTIMI_OVERFLOW_ERROR = 0,
    /** Discard the new message when the queue is full. */
    OPTIMI_OVERFLOW_DROP_NEWEST,
    /** Discard the oldest message in the queue to make room for the new message. */
    OPTIMI_OVERFLOW_DROP_OLDEST
} optimi_queue_overflow_policy_t;

/**
 * @brief Flush behavior for buffered logging.
 */
typedef enum {
    /** Flush after each write for maximum durability. */
    OPTIMI_FLUSH_MODE_IMMEDIATE,
    /** Flush periodically based on configured interval. */
    OPTIMI_FLUSH_MODE_BALANCED
} optimi_flush_mode_t;

/**
 * @brief Flushing behavior when shutting down the logger.
 */
typedef enum {
    /** write all pending messages before returning. */
    OPTIMI_SHUTDOWN_FLUSH_QUEUE = 0,
    /** Attempt to flush the queue until timeout expires. */
    OPTIMI_SHUTDOWN_BEST_EFFORT,
    /** Drop any pending messages and stop quickly. */
    OPTIMI_SHUTDOWN_DROP_QUEUE
} optimi_shutdown_mode_t;

/**
 * @brief Configuration used when creating a logger instance.
 */
typedef struct {
    /** Directory or full path used for log files. */
    const char* output_path;
    /** Name (prefix) for generated log filenames. */
    const char* filename_prefix;
    /** If non-zero, append date stamp (YYYYMMDD) to filename. */
    int timestamp_in_filename;
    /** Minimum level accepted by logger. Lower levels are filtered out. */
    optimi_log_level_t min_level;
    /** If non-zero, enable console output. */
    int enable_console;
    /** If non-zero, enable file output. */
    int enable_file;
    /** If non-zero, apply ANSI colors to console output. */
    int enable_colors;
    /** Capacity of message queue.*/
    size_t queue_size;
    /** Overflow strategy when queue_size is exhausted. */
    optimi_queue_overflow_policy_t overflow_policy;
    /** Flush policy for buffered data. */
    optimi_flush_mode_t flush_mode;
    /** Flush period in milliseconds for balanced mode. */
    uint32_t flush_interval_ms;
} optimi_logger_config_t;

/**
 * @brief Fill a config struct with library defaults.
 *
 * @param config Pointer to config object to initialize.
 */
void optimi_logger_config_init_default(optimi_logger_config_t* config);

/**
 * @brief Create and initialize a logger instance.
 *
 * @param[out] out_logger Receives allocated logger instance on success.
 * @param[in] config User configuration. Must not be NULL.
 * @return Status code describing success or failure.
 */
optimi_status_t optimi_logger_create(
    optimi_logger_t** out_logger,
    const optimi_logger_config_t* config
);

/**
 * @brief Destroy a logger instance and release resources.
 *
 * @param logger Logger instance returned by optimi_logger_create().
 */
void optimi_logger_destroy(optimi_logger_t* logger);

/**
 * @brief Update minimum accepted log level at runtime.
 *
 * @param logger Logger instance.
 * @param min_level New minimum level.
 * @return Status code describing success or failure.
 */
optimi_status_t optimi_logger_set_min_level(
    optimi_logger_t* logger,
    optimi_log_level_t min_level
);

/**
 * @brief Flush pending buffered log data.
 *
 * @param logger Logger instance.
 * @return Status code describing success or failure.
 */
optimi_status_t optimi_logger_flush(optimi_logger_t* logger);

/**
 * @brief Stop logger processing and optionally drain pending messages.
 *
 * @param logger Logger instance.
 * @param timeout_ms Maximum wait time for drain operations.
 * @param mode Shutdown strategy.
 * @return Status code describing success or failure.
 */
optimi_status_t optimi_logger_shutdown(
    optimi_logger_t* logger,
    uint32_t timeout_ms,
    optimi_shutdown_mode_t mode
);

/**
 * @brief Log a formatted message with caller metadata.
 *
 * Typical callers should prefer OPTIMI_LOG_* macros to automatically provide
 * file/function/line context.
 *
 * @param logger Logger instance.
 * @param level Log severity.
 * @param file Source file path of the call site.
 * @param function Function name of the call site.
 * @param line Source line number of the call site.
 * @param format printf-style format string.
 * @param ... Format arguments.
 * @return Status code describing success or failure.
 */
optimi_status_t optimi_logger_log(
    optimi_logger_t* logger,
    optimi_log_level_t level,
    const char* file,
    const char* function,
    int line,
    const char* format,
    ...
);

/**
 * @brief va_list overload of optimi_logger_log().
 *
 * @param logger Logger instance.
 * @param level Log severity.
 * @param file Source file path of the call site.
 * @param function Function name of the call site.
 * @param line Source line number of the call site.
 * @param format printf-style format string.
 * @param args Variadic argument list.
 * @return Status code describing success or failure.
 */
optimi_status_t optimi_logger_logv(
    optimi_logger_t* logger,
    optimi_log_level_t level,
    const char* file,
    const char* function,
    int line,
    const char* format,
    va_list args
);

/**
 * @brief Convert status code to a readable constant string.
 *
 * @param status Status code.
 * @return Non-NULL string describing status.
 */
const char* optimi_logger_status_string(optimi_status_t status);

/**
 * @brief Log DEBUG message with automatic caller metadata.
 */
#define OPTIMI_LOG_DEBUG(logger, fmt, ...) \
    optimi_logger_log((logger), OPTIMI_LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, (fmt), ##__VA_ARGS__)
/**
 * @brief Log INFO message with automatic caller metadata.
 */
#define OPTIMI_LOG_INFO(logger, fmt, ...) \
    optimi_logger_log((logger), OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, (fmt), ##__VA_ARGS__)
/**
 * @brief Log WARNING message with automatic caller metadata.
 */
#define OPTIMI_LOG_WARNING(logger, fmt, ...) \
    optimi_logger_log((logger), OPTIMI_LOG_LEVEL_WARN, __FILE__, __func__, __LINE__, (fmt), ##__VA_ARGS__)
/**
 * @brief Log ERROR message with automatic caller metadata.
 */
#define OPTIMI_LOG_ERROR(logger, fmt, ...) \
    optimi_logger_log((logger), OPTIMI_LOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, (fmt), ##__VA_ARGS__)
/**
 * @brief Log FATAL message with automatic caller metadata.
 */
#define OPTIMI_LOG_FATAL(logger, fmt, ...) \
    optimi_logger_log((logger), OPTIMI_LOG_LEVEL_FATAL, __FILE__, __func__, __LINE__, (fmt), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif