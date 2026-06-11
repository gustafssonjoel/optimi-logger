#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "optimi_logger.h"

#define NUM_THREADS 4
#define MESSAGES_PER_THREAD 100

typedef struct {
    optimi_logger_t* logger;
    int thread_id;
    int message_count;
    int errors;
} thread_context_t;

static void* logging_thread(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    int i;

    for (i = 0; i < ctx->message_count; ++i) {
        optimi_status_t status = optimi_logger_log(
            ctx->logger,
            OPTIMI_LOG_LEVEL_INFO,
            __FILE__,
            __func__,
            __LINE__,
            "Thread %d message %d",
            ctx->thread_id,
            i
        );
        if (status != OPTIMI_STATUS_OK && status != OPTIMI_STATUS_QUEUE_FULL) {
            ctx->errors++;
        }
    }
    return NULL;
}

static int test_concurrent_logging(void) {
    optimi_logger_config_t config;
    optimi_logger_t* logger;
    pthread_t threads[NUM_THREADS];
    thread_context_t contexts[NUM_THREADS];
    int i, errors = 0;

    printf("[TEST] Concurrent logging from %d threads\n", NUM_THREADS);

    optimi_logger_config_init_default(&config);
    config.queue_size = 1000;
    config.enable_console = 1;
    config.enable_file = 0;

    if (optimi_logger_create(&logger, &config) != OPTIMI_STATUS_OK) {
        printf("  FAIL: Logger creation failed\n");
        return 1;
    }

    /* Create threads */
    for (i = 0; i < NUM_THREADS; ++i) {
        contexts[i].logger = logger;
        contexts[i].thread_id = i;
        contexts[i].message_count = MESSAGES_PER_THREAD;
        contexts[i].errors = 0;

        if (pthread_create(&threads[i], NULL, logging_thread, &contexts[i]) != 0) {
            printf("  FAIL: Thread creation failed\n");
            optimi_logger_destroy(logger);
            return 1;
        }
    }

    /* Wait for all threads */
    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
        errors += contexts[i].errors;
    }

    /* Flush and clean up */
    optimi_logger_flush(logger);
    optimi_logger_destroy(logger);

    if (errors == 0) {
        printf("  PASS: All threads logged without critical errors\n");
        return 0;
    } else {
        printf("  FAIL: %d logging errors across threads\n", errors);
        return 1;
    }
}

static int test_concurrent_level_updates(void) {
    optimi_logger_config_t config;
    optimi_logger_t* logger;
    pthread_t threads[NUM_THREADS];
    thread_context_t contexts[NUM_THREADS];
    int i, errors = 0;

    printf("[TEST] Concurrent level updates\n");

    optimi_logger_config_init_default(&config);
    config.queue_size = 500;
    config.enable_console = 0;
    config.enable_file = 0;

    if (optimi_logger_create(&logger, &config) != OPTIMI_STATUS_OK) {
        printf("  FAIL: Logger creation failed\n");
        return 1;
    }

    /* Create logging threads */
    for (i = 0; i < NUM_THREADS; ++i) {
        contexts[i].logger = logger;
        contexts[i].thread_id = i;
        contexts[i].message_count = MESSAGES_PER_THREAD;
        contexts[i].errors = 0;

        if (pthread_create(&threads[i], NULL, logging_thread, &contexts[i]) != 0) {
            printf("  FAIL: Thread creation failed\n");
            optimi_logger_destroy(logger);
            return 1;
        }
    }

    /* Main thread updates log level while others log */
    for (i = 0; i < 10; ++i) {
        optimi_log_level_t level = (i % 2) ? OPTIMI_LOG_LEVEL_DEBUG : OPTIMI_LOG_LEVEL_ERROR;
        if (optimi_logger_set_min_level(logger, level) != OPTIMI_STATUS_OK) {
            errors++;
        }
        usleep(10000); /* 10ms */
    }

    /* Wait for logging threads */
    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
        errors += contexts[i].errors;
    }

    optimi_logger_destroy(logger);

    if (errors == 0) {
        printf("  PASS: Level updates and logging were synchronized\n");
        return 0;
    } else {
        printf("  FAIL: %d errors during concurrent level updates\n", errors);
        return 1;
    }
}

static int test_post_shutdown_rejection(void) {
    optimi_logger_config_t config;
    optimi_logger_t* logger;
    optimi_status_t status;

    printf("[TEST] Logging after shutdown is rejected\n");

    optimi_logger_config_init_default(&config);
    config.enable_console = 0;
    config.enable_file = 0;

    if (optimi_logger_create(&logger, &config) != OPTIMI_STATUS_OK) {
        printf("  FAIL: Logger creation failed\n");
        return 1;
    }

    /* Log before shutdown */
    status = optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "before");
    if (status != OPTIMI_STATUS_OK) {
        printf("  FAIL: Logging before shutdown failed\n");
        optimi_logger_destroy(logger);
        return 1;
    }

    /* Shutdown */
    optimi_logger_shutdown(logger, 5000, OPTIMI_SHUTDOWN_FLUSH_QUEUE);

    /* Try to log after shutdown */
    status = optimi_logger_log(logger, OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "after");
    if (status == OPTIMI_STATUS_NOT_INITIALIZED) {
        printf("  PASS: Post-shutdown logging correctly rejected\n");
        return 0;
    } else {
        printf("  FAIL: Expected NOT_INITIALIZED, got %d\n", status);
        return 1;
    }
}

int main(void) {
    int passed = 0, failed = 0;

    printf("Threading tests (runnable implementations):\n\n");

    if (test_concurrent_logging() == 0) {
        passed++;
    } else {
        failed++;
    }

    if (test_concurrent_level_updates() == 0) {
        passed++;
    } else {
        failed++;
    }

    if (test_post_shutdown_rejection() == 0) {
        passed++;
    } else {
        failed++;
    }

    printf("\nSummary: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
