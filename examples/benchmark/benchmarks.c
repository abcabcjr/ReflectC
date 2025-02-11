#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <reflect.h>

#define DEFAULT_NUM_STRUCTS 2000
#define DEFAULT_NUM_RUNS    10000
#define TYPE_NAME_SIZE      32

static double get_time_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (ts.tv_sec * 1e6) + (ts.tv_nsec / 1e3);
}

typedef struct {
    int num_structs;
    char **type_names;
} bench_config_t;

static bench_config_t g_cfg = { DEFAULT_NUM_STRUCTS, NULL };

static void init_type_names(void) {
    const int n = g_cfg.num_structs;
    g_cfg.type_names = malloc(n * sizeof(char *));
    if (!g_cfg.type_names) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; i++) {
        g_cfg.type_names[i] = malloc(TYPE_NAME_SIZE);
        if (!g_cfg.type_names[i]) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        snprintf(g_cfg.type_names[i], TYPE_NAME_SIZE, "struct_%d", i);
    }
}

static void free_type_names(void) {
    for (int i = 0; i < g_cfg.num_structs; i++) {
        free(g_cfg.type_names[i]);
    }
    free(g_cfg.type_names);
}

typedef void (*benchmark_func_t)(void);

typedef struct {
    const char *name;
    benchmark_func_t func;
} benchmark_t;

static void bench_reflect_load(void) {
    reflect_load();
}

static void bench_reflect_type_info(void) {
    for (int i = 0; i < g_cfg.num_structs; i++) {
        const volatile type_info_t* info = reflect_type_info_from_name(g_cfg.type_names[i]);
    }
}

static void bench_reflect_field_info(void) {
    for (int i = 1; i < g_cfg.num_structs - 1; i++) {
        const volatile type_info_t* info = reflect_type_info_from_name(g_cfg.type_names[i]);
        const volatile field_info_t* field_iter = reflect_field_info_iter_begin(info);
        if (field_iter && field_iter->name) {
            const volatile field_info_t* field_info = reflect_get_field_type(info, field_iter->name);
        }
    }
}

static void run_benchmark(const benchmark_t *bm, int num_runs) {
    double *times = malloc(num_runs * sizeof(double));
    if (!times) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_runs; i++) {
        const double start = get_time_us();
        bm->func();
        const double end = get_time_us();
        times[i] = end - start;
    }

    double sum = 0.0, min = times[0], max = times[0];
    for (int i = 0; i < num_runs; i++) {
        sum += times[i];
        if (times[i] < min) min = times[i];
        if (times[i] > max) max = times[i];
    }

    const double mean = sum / num_runs;

    printf("Benchmark '%s' run %d times: mean = %.3f µs, min = %.3f µs, max = %.3f µs\n",
           bm->name, num_runs, mean, min, max);

    free(times);
}

/*
 *   Usage: ./benchmark [num_structs] [num_runs]
 */
int main(int argc, char *argv[]) {
    int num_runs = DEFAULT_NUM_RUNS;

    if (argc > 1) {
        int n = atoi(argv[1]);
        if (n > 0) {
            g_cfg.num_structs = n;
        } else {
            fprintf(stderr, "Invalid number of structs specified: %s\n", argv[1]);
            return 1;
        }
    }

    if (argc > 2) {
        int runs = atoi(argv[2]);
        if (runs > 0) {
            num_runs = runs;
        } else {
            fprintf(stderr, "Invalid number of runs specified: %s\n", argv[2]);
            return 1;
        }
    }

    init_type_names();

    const benchmark_t benchmarks[] = {
        { "reflect_load",                bench_reflect_load },
        { "reflect_type_info_from_name", bench_reflect_type_info },
        { "reflect_get_field_type",      bench_reflect_field_info }
    };

    const size_t numBenchmarks = sizeof(benchmarks) / sizeof(benchmark_t);

    for (size_t i = 0; i < numBenchmarks; i++) {
        printf("Running benchmark: %s\n", benchmarks[i].name);
        run_benchmark(&benchmarks[i], num_runs);
    }

    free_type_names();
    return 0;
}
