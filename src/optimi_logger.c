#include "optimi_logger.h"

/*
 * Template implementation file for the public logger API.
 * All functions are intentionally stubbed and must be implemented.
 */

void optimi_logger_config_init_default(optimi_logger_config_t* config)
{
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
    (void)out_logger;
    (void)config;
    /* TODO: allocate and initialize logger instance. */
    return OPTIMI_STATUS_UNIMPLEMENTED;
}

void optimi_logger_destroy(optimi_logger_t* logger)
{
    (void)logger;
    /* TODO: release logger resources. */
}

optimi_status_t optimi_logger_set_min_level(
    optimi_logger_t* logger,
    optimi_log_level_t min_level
)
{
    (void)logger;
    (void)min_level;
    /* TODO: update runtime min log level. */
    return OPTIMI_STATUS_UNIMPLEMENTED;
}

optimi_status_t optimi_logger_flush(optimi_logger_t* logger)
{
    (void)logger;
    /* TODO: flush buffered output. */
    return OPTIMI_STATUS_UNIMPLEMENTED;
}

optimi_status_t optimi_logger_shutdown(
    optimi_logger_t* logger,
    uint32_t timeout_ms,
    optimi_shutdown_mode_t mode
)
{
    (void)logger;
    (void)timeout_ms;
    (void)mode;
    /* TODO: stop worker and apply shutdown behavior. */
    return OPTIMI_STATUS_UNIMPLEMENTED;
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
    (void)logger;
    (void)level;
    (void)file;
    (void)function;
    (void)line;
    (void)format;
    /* TODO: format message and enqueue/write it. */
    return OPTIMI_STATUS_UNIMPLEMENTED;
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
    (void)logger;
    (void)level;
    (void)file;
    (void)function;
    (void)line;
    (void)format;
    (void)args;
    /* TODO: process va_list and enqueue/write message. */
    return OPTIMI_STATUS_UNIMPLEMENTED;
}

const char* optimi_logger_status_string(optimi_status_t status)
{
    (void)status;
    /* TODO: map status values to readable strings. */
    return "OPTIMI_STATUS_UNIMPLEMENTED";
}
