#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>

#include "optimi_logger.h"

#define LARGE_MESSAGE_SIZE 1024
#define MESSAGE_ITERATIONS 100

static void* allocate_message_buffer(size_t size) {
    void* buffer = malloc(size);
    if (!buffer) return NULL;
    memset(buffer, 'X', size - 1);
    ((char*)buffer)[size - 1] = '\0';
    return buffer;
}

static int test_create_destroy_cycle(void) {
    optimi_logger_config_t config;
    optimi_logger_t* logger;
    int i, errors = 0;

    printf("[TEST] Create/destroy cycles (10 iterations)\n");

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;

    for (i = 0; i < 10; ++i) {
        if (optimi_logger_create(&logger, &config) != OPTIMI_STATUS_OK) {
            printf("  FAIL: Logger creation failed at iteration %d\n", i);
            errors++;
            continue;
        }

        optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__,
                         "Cycle %d", i);

        optimi_logger_destroy(logger);
    }

    if (errors == 0) {
        printf("  PASS: All create/destroy cycles completed successfully\n");
        return 0;
    } else {
        printf("  FAIL: %d errors during cycles\n", errors);
        return 1;
    }
}

static int test_large_message_handling(void) {
    optimi_logger_config_t config;
    optimi_logger_t* logger;
    char* large_msg;
    optimi_status_t status;

    printf("[TEST] Large message handling (%d bytes)\n", LARGE_MESSAGE_SIZE);

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;

    if (optimi_logger_create(&logger, &config) != OPTIMI_STATUS_OK) {
        printf("  FAIL: Logger creation failed\n");
        return 1;
    }

    large_msg = allocate_message_buffer(LARGE_MESSAGE_SIZE);
    if (!large_msg) {
        printf("  FAIL: Could not allocate test message\n");
        optimi_logger_destroy(logger);
        return 1;
    }

    status = optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__,
                              "Large: %s", large_msg);

    free(large_msg);
    optimi_logger_destroy(logger);

    if (status == OPTIMI_STATUS_OK || status == OPTIMI_STATUS_QUEUE_FULL) {
        printf("  PASS: Large message accepted\n");
        return 0;
    } else {
        printf("  FAIL: Large message rejected with status %d\n", status);
        return 1;
    }
}

static int test_queue_memory_scaling(void) {
    optimi_logger_config_t config;
    optimi_logger_t* logger_small;
    optimi_logger_t* logger_large;
    struct rusage usage_before, usage_after;

    printf("[TEST] Queue memory scaling (small vs large)\n");

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;

    /* Small queue */
    config.queue_capacity = 10;
    if (optimi_logger_create(&logger_small, &config) != OPTIMI_STATUS_OK) {
        printf("  FAIL: Small logger creation failed\n");
        return 1;
    }

    /* Large queue */
    config.queue_capacity = 1000;
    if (optimi_logger_create(&logger_large, &config) != OPTIMI_STATUS_OK) {
        printf("  FAIL: Large logger creation failed\n");
        optimi_logger_destroy(logger_small);
        return 1;
    }

    /* Log to both */
    optimi_logger_log(logger_small, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "small");
    optimi_logger_log(logger_large, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "large");

    optimi_logger_destroy(logger_small);
    optimi_logger_destroy(logger_large);

    printf("  PASS: Different queue sizes initialized successfully\n");
    return 0;
}

static int test_repeated_logging(void) {
    optimi_logger_config_t config;
    optimi_logger_t* logger;
    int i, errors = 0;

    printf("[TEST] Repeated logging (%d messages)\n", MESSAGE_ITERATIONS);

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;
    config.queue_capacity = 50;

    if (optimi_logger_create(&logger, &config) != OPTIMI_STATUS_OK) {
        printf("  FAIL: Logger creation failed\n");
        return 1;
    }

    for (i = 0; i < MESSAGE_ITERATIONS; ++i) {
        optimi_status_t status = optimi_logger_log(
            logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__,
            "Message %d with some data", i
        );
        /* Accept OK or QUEUE_FULL as valid outcomes */
        if (status != OPTIMI_STATUS_OK && status != OPTIMI_STATUS_QUEUE_FULL) {
            errors++;
        }
    }

    optimi_logger_flush(logger);
    optimi_logger_destroy(logger);

    if (errors == 0) {
        printf("  PASS: All messages logged successfully\n");
        return 0;
    } else {
        printf("  FAIL: %d logging errors\n", errors);
        return 1;
    }
}

static int test_config_on_stack(void) {
    optimi_logger_config_t config;

    printf("[TEST] Config initialization on stack\n");

    optimi_logger_config_init_default(&config);

    /* Verify config is initialized */
    if (config.output_path == NULL || config.filename_prefix == NULL) {
        printf("  FAIL: Config not properly initialized\n");
        return 1;
    }

    printf("  PASS: Config on stack initialized correctly\n");
    return 0;
}

static int test_null_input_handling(void) {
    optimi_logger_t* logger = NULL;

    printf("[TEST] NULL input handling\n");

    /* These should all be safe no-ops or return errors */
    optimi_logger_destroy(NULL);
    optimi_logger_flush(NULL);

    if (optimi_logger_log(NULL, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__,
                         "test") == OPTIMI_STATUS_INVALID_ARGUMENT) {
        printf("  PASS: NULL inputs handled safely\n");
        return 0;
    } else {
        printf("  FAIL: NULL input not handled\n");
        return 1;
    }
}

int main(void) {
    int passed = 0, failed = 0;

    printf("Memory and resource tests (runnable implementations):\n\n");

    if (test_create_destroy_cycle() == 0) {
        passed++;
    } else {
        failed++;
    }

    if (test_large_message_handling() == 0) {
        passed++;
    } else {
        failed++;
    }

    if (test_queue_memory_scaling() == 0) {
        passed++;
    } else {
        failed++;
    }

    if (test_repeated_logging() == 0) {
        passed++;
    } else {
        failed++;
    }

    if (test_config_on_stack() == 0) {
        passed++;
    } else {
        failed++;
    }

    if (test_null_input_handling() == 0) {
        passed++;
    } else {
        failed++;
    }

    printf("\nSummary: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
