#include <stdlib.h>
#include "optimi_logger.h"

struct optimi_logger {
    optimi_logger_config_t config;
    int is_shutdown;
};

void optimi_logger_config_init_default(optimi_logger_config_t* config) {
    if (!config) return;
    config->output_path = ".";
    config->filename_prefix = "optimi";
    config->timestamp_in_filename = 0;
    config->min_level = OPTIMI_LOG_LEVEL_INFO;
    config->enable_console = 1;
    config->enable_file = 0;
    config->enable_colors = 0;
    config->queue_size = 256;
    config->overflow_policy = OPTIMI_OVERFLOW_ERROR;
    config->flush_mode = OPTIMI_FLUSH_MODE_IMMEDIATE;
    config->flush_interval_ms = 1000;
}

optimi_status_t optimi_logger_create(
    optimi_logger_t** out_logger,
    const optimi_logger_config_t* config
) {
    if (!out_logger || !config) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }
    
    optimi_logger_t* logger = malloc(sizeof(optimi_logger_t));
    if (!logger) {
        return OPTIMI_STATUS_ALLOCATION_FAILED;
    }
    
    logger->config = *config;
    logger->is_shutdown = 0;
    *out_logger = logger;
    return OPTIMI_STATUS_OK;
}

void optimi_logger_destroy(optimi_logger_t* logger) {
    if (logger) {
        free(logger);
    }
}

optimi_status_t optimi_logger_set_min_level(
    optimi_logger_t* logger,
    optimi_log_level_t min_level
) {
    if (!logger) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }
    logger->config.min_level = min_level;
    return OPTIMI_STATUS_OK;
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
    (void)level;
    (void)file;
    (void)function;
    (void)line;
    (void)format;
    
    if (!logger || !format) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }
    if (logger->is_shutdown) {
        return OPTIMI_STATUS_NOT_INITIALIZED;
    }
    return OPTIMI_STATUS_OK;
}

optimi_status_t optimi_logger_logv(
    optimi_logger_t* logger,
    optimi_log_level_t level,
    const char* file,
    const char* function,
    int line,
    const char* format,
    va_list args
) {
    (void)logger;
    (void)level;
    (void)file;
    (void)function;
    (void)line;
    (void)format;
    (void)args;
    
    if (!logger || !format) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }
    return OPTIMI_STATUS_OK;
}

optimi_status_t optimi_logger_flush(optimi_logger_t* logger) {
    if (!logger) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }
    return OPTIMI_STATUS_OK;
}

optimi_status_t optimi_logger_shutdown(
    optimi_logger_t* logger,
    uint32_t timeout_ms,
    optimi_shutdown_mode_t mode
) {
    (void)timeout_ms;
    (void)mode;
    
    if (!logger) {
        return OPTIMI_STATUS_INVALID_ARGUMENT;
    }
    logger->is_shutdown = 1;
    return OPTIMI_STATUS_OK;
}
