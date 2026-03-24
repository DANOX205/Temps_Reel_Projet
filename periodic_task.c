#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

// Prototypes
void set_task_period(struct timespec *task_period, long period_ns);
void sleep_until_next_activation(struct timespec *next_activation);
void process_one_activation(void);
void timespec_add(const struct timespec *a,const struct timespec *b,struct timespec *res);

int main(int argc, char **argv) {
    struct timespec next_activation, task_period;
    struct timespec job_release, job_end;
    long exec_ns;
    int err;

    // Période = 5 secondes = 5 000 000 000 ns
    set_task_period(&task_period, 5000000000L);

    // Première activation : maintenant
    err = clock_gettime(CLOCK_MONOTONIC, &next_activation);
    assert(err == 0);

    while (1) {
        // Temps de début de job
        err = clock_gettime(CLOCK_MONOTONIC, &job_release);
        assert(err == 0);
        process_one_activation();
        
        // Temps de fin de job
        err = clock_gettime(CLOCK_MONOTONIC, &job_end);
        assert(err == 0);

        // Temps d'exécution en nanosecondes
        exec_ns = (job_end.tv_sec - job_release.tv_sec) * 1000000000L
                  + (job_end.tv_nsec - job_release.tv_nsec);

        printf("Execution time: %ld ns (%.6f ms)\n",exec_ns, exec_ns / 1e6);

        // Calcul de la prochaine activation
        timespec_add(&next_activation, &task_period, &next_activation);

        // Attente jusqu'à la prochaine activation (absolue)
        sleep_until_next_activation(&next_activation);
    }

    return 0;
}

void set_task_period(struct timespec *task_period, long period_ns) {
    task_period->tv_sec  = period_ns / 1000000000L;
    task_period->tv_nsec = period_ns % 1000000000L;
}

void sleep_until_next_activation(struct timespec *next_activation) {
    int err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,next_activation,NULL);
    } while (err != 0 && errno == EINTR);
    assert(err == 0);
}

void process_one_activation(void) {
    int load = 10000;
    volatile int i; // volatile pour éviter l'optimisation
    for (i = 0; i < load * 1000; i++) {
        // charge CPU
    }
    printf("Hello real-time world! i = %d\n", i);
}

void timespec_add(const struct timespec *a,const struct timespec *b,struct timespec *res) {
    res->tv_sec  = a->tv_sec  + b->tv_sec;
    res->tv_nsec = a->tv_nsec + b->tv_nsec;

    if (res->tv_nsec >= 1000000000L) {
        res->tv_sec += 1;
        res->tv_nsec -= 1000000000L;
    }
}
