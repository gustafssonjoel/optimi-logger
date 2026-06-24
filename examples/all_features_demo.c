#include "optimi_logger.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    optimi_logger_t* logger;
    int worker_id;
    int iterations;
} worker_args_t;

static int require_ok(const char* action, optimi_status_t status)
{
    if (status == OPTIMI_STATUS_OK) {
        return 0;
    }

    fprintf(
        stderr,
        "[demo] %s failed: %s\n",
        action,
        optimi_logger_status_string(status)
    );
    return -1;
}

static void* worker_thread(void* arg)
{
    worker_args_t* worker = (worker_args_t*)arg;
    int i;

    for (i = 0; i < worker->iterations; ++i) {
        (void)OPTIMI_LOG_DEBUG(
            worker->logger,
            "worker=%d debug iteration=%d",
            worker->worker_id,
            i
        );
        (void)OPTIMI_LOG_INFO(
            worker->logger,
            "worker=%d info iteration=%d",
            worker->worker_id,
            i
        );

        if ((i % 5) == 0) {
            (void)OPTIMI_LOG_WARNING(
                worker->logger,
                "worker=%d warning checkpoint=%d",
                worker->worker_id,
                i
            );
        }

        if ((i % 11) == 0) {
            (void)OPTIMI_LOG_ERROR(
                worker->logger,
                "worker=%d error checkpoint=%d",
                worker->worker_id,
                i
            );
        }

        usleep(20000);
    }

    return NULL;
}

static int run_main_feature_demo(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;
    pthread_t threads[3];
    worker_args_t args[3];
    int i;

    optimi_logger_config_init_default(&config);
    config.output_path = "build/logs";
    config.filename_prefix = "all_features";
    config.timestamp_in_filename = 1;
    config.min_level = OPTIMI_LOG_LEVEL_DEBUG;
    config.enable_console = 1;
    config.enable_file = 1;
    config.enable_colors = 1;
    config.queue_capacity = 32;
    config.overflow_policy = OPTIMI_OVERFLOW_DROP_OLDEST;
    config.flush_mode = OPTIMI_FLUSH_MODE_BALANCED;
    config.flush_interval_ms = 250;

    if (require_ok("create main logger", optimi_logger_create(&logger, &config)) != 0) {
        return -1;
    }

    (void)OPTIMI_LOG_INFO(
        logger,
        "main demo started: console+file+color+timestamp+balanced flush enabled"
    );

    for (i = 0; i < 3; ++i) {
        args[i].logger = logger;
        args[i].worker_id = i + 1;
        args[i].iterations = 30;

        if (pthread_create(&threads[i], NULL, worker_thread, &args[i]) != 0) {
            perror("pthread_create");
            (void)optimi_logger_shutdown(logger, 1000, OPTIMI_SHUTDOWN_BEST_EFFORT);
            optimi_logger_destroy(logger);
            return -1;
        }
    }

    sleep(1);

    if (require_ok(
            "set min level WARN",
            optimi_logger_set_min_level(logger, OPTIMI_LOG_LEVEL_WARN)
        ) != 0) {
        (void)optimi_logger_shutdown(logger, 1000, OPTIMI_SHUTDOWN_BEST_EFFORT);
        optimi_logger_destroy(logger);
        return -1;
    }

    (void)OPTIMI_LOG_INFO(logger, "this INFO should be filtered by WARN min level");
    (void)OPTIMI_LOG_WARNING(logger, "min-level filtering is now active at WARN");

    if (require_ok(
            "set min level DEBUG",
            optimi_logger_set_min_level(logger, OPTIMI_LOG_LEVEL_DEBUG)
        ) != 0) {
        (void)optimi_logger_shutdown(logger, 1000, OPTIMI_SHUTDOWN_BEST_EFFORT);
        optimi_logger_destroy(logger);
        return -1;
    }

    (void)OPTIMI_LOG_FATAL(logger, "fatal event example after restoring DEBUG min level");

    for (i = 0; i < 3; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
        }
    }

    if (require_ok("manual flush", optimi_logger_flush(logger)) != 0) {
        (void)optimi_logger_shutdown(logger, 1000, OPTIMI_SHUTDOWN_BEST_EFFORT);
        optimi_logger_destroy(logger);
        return -1;
    }

    if (require_ok(
            "best-effort shutdown",
            optimi_logger_shutdown(logger, 1000, OPTIMI_SHUTDOWN_BEST_EFFORT)
        ) != 0) {
        optimi_logger_destroy(logger);
        return -1;
    }

    optimi_logger_destroy(logger);
    return 0;
}

static int run_overflow_policy_demo(optimi_queue_overflow_policy_t policy, const char* name)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;
    optimi_status_t status;
    int i;

    optimi_logger_config_init_default(&config);
    config.output_path = "build/logs";
    config.filename_prefix = name;
    config.timestamp_in_filename = 0;
    config.min_level = OPTIMI_LOG_LEVEL_DEBUG;
    config.enable_console = 0;
    config.enable_file = 1;
    config.enable_colors = 0;
    config.queue_capacity = 3;
    config.overflow_policy = policy;
    config.flush_mode = OPTIMI_FLUSH_MODE_BALANCED;
    config.flush_interval_ms = 5000;

    if (require_ok("create overflow logger", optimi_logger_create(&logger, &config)) != 0) {
        return -1;
    }

    for (i = 0; i < 8; ++i) {
        status = OPTIMI_LOG_INFO(logger, "%s message %d", name, i);
        if (policy == OPTIMI_OVERFLOW_ERROR && status == OPTIMI_STATUS_QUEUE_FULL) {
            fprintf(stderr, "[demo] expected queue-full observed for %s\n", name);
            break;
        }
    }

    if (require_ok("flush overflow logger", optimi_logger_flush(logger)) != 0) {
        (void)optimi_logger_shutdown(logger, 0, OPTIMI_SHUTDOWN_DROP_QUEUE);
        optimi_logger_destroy(logger);
        return -1;
    }

    if (require_ok(
            "flush-queue shutdown",
            optimi_logger_shutdown(logger, 0, OPTIMI_SHUTDOWN_FLUSH_QUEUE)
        ) != 0) {
        optimi_logger_destroy(logger);
        return -1;
    }

    optimi_logger_destroy(logger);
    return 0;
}

static int run_drop_shutdown_demo(void)
{
    optimi_logger_config_t config;
    optimi_logger_t* logger = NULL;

    optimi_logger_config_init_default(&config);
    config.output_path = "build/logs";
    config.filename_prefix = "shutdown_drop_queue";
    config.enable_console = 1;
    config.enable_file = 0;
    config.enable_colors = 0;
    config.queue_capacity = 8;
    config.overflow_policy = OPTIMI_OVERFLOW_DROP_NEWEST;
    config.flush_mode = OPTIMI_FLUSH_MODE_BALANCED;
    config.flush_interval_ms = 10000;

    if (require_ok("create drop-shutdown logger", optimi_logger_create(&logger, &config)) != 0) {
        return -1;
    }

    (void)OPTIMI_LOG_INFO(logger, "this message may be dropped by DROP_QUEUE shutdown mode");

    if (require_ok(
            "drop-queue shutdown",
            optimi_logger_shutdown(logger, 0, OPTIMI_SHUTDOWN_DROP_QUEUE)
        ) != 0) {
        optimi_logger_destroy(logger);
        return -1;
    }

    optimi_logger_destroy(logger);
    return 0;
}

int main(void)
{
    if (run_main_feature_demo() != 0) {
        return EXIT_FAILURE;
    }

    if (run_overflow_policy_demo(OPTIMI_OVERFLOW_ERROR, "overflow_error") != 0) {
        return EXIT_FAILURE;
    }

    if (run_overflow_policy_demo(OPTIMI_OVERFLOW_DROP_NEWEST, "overflow_drop_newest") != 0) {
        return EXIT_FAILURE;
    }

    if (run_overflow_policy_demo(OPTIMI_OVERFLOW_DROP_OLDEST, "overflow_drop_oldest") != 0) {
        return EXIT_FAILURE;
    }

    if (run_drop_shutdown_demo() != 0) {
        return EXIT_FAILURE;
    }

    puts("all_features_demo complete. Inspect build/logs for generated log files.");
    return EXIT_SUCCESS;
}