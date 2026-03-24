#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>

// =========================
//  UTILITAIRES TEMPS REEL
// =========================

// Convertit une période en nanosecondes → struct timespec
void set_task_period(struct timespec *period, long period_ns) {
    period->tv_sec  = period_ns / 1000000000L;
    period->tv_nsec = period_ns % 1000000000L;
}

// Additionne deux timespec
void timespec_add(const struct timespec *a,const struct timespec *b,struct timespec *res) {
    res->tv_sec  = a->tv_sec  + b->tv_sec;
    res->tv_nsec = a->tv_nsec + b->tv_nsec;

    if (res->tv_nsec >= 1000000000L) {
        res->tv_sec += 1;
        res->tv_nsec -= 1000000000L;
    }
}

// Sleep absolu jusqu'à next_activation
void sleep_until_next_activation(const struct timespec *next_activation) {
    int err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,next_activation,NULL);
    } while (err != 0 && errno == EINTR);

    assert(err == 0);
}

// =========================
//  PARAMETRES
// =========================
pthread_barrier_t barrier;
pthread_mutex_t mutex;

// =========================
//  TACHE PERIODIQUE
// =========================

typedef struct {
  int id;
  long period_ns;
  int load;
} task_param_t;

void *task_function(void *arg){
  task_param_t *param = (task_param_t *) arg;
  struct timespec next_activation;
  struct timespec period;
  struct timespec start, end;
  
  set_task_period(&period, param->period_ns);
  clock_gettime(CLOCK_MONOTONIC, &next_activation);
  
  //Synchronisation
  pthread_barrier_wait(&barrier);
  
  while(1){
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // ==== SECTION CRITIQUE 
    if (param->id == 1 || param->id == 2){
      pthread_mutex_lock(&mutex);
      for (int i=0; i<param->load * 1000;i++){
        /* do nothing, keep counting */
      }
      pthread_mutex_unlock(&mutex);
    } else { // Pas de mutex pour T3
      for (int i=0; i<param->load * 1000;i++){
        /* do nothing, keep counting */
      }
    }
    
    clock_gettime(CLOCK_MONOTONIC,&end);
    long exec_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
    printf("[Task %d]Execution time: %ld ns (%.3f ms)\n", param->id, exec_ns, exec_ns / 1e6);
    
    timespec_add(&next_activation,&period,&next_activation);
    sleep_until_next_activation(&next_activation);
  }
}


// =========================
//  PROGRAMME PRINCIPAL
// =========================

int main(int argc, char **argv) {
  pthread_t t1, t2, t3;
  task_param_t p1 = {1,1000000000L,10000}; // 1s
  task_param_t p2 = {2,2000000000L,10000}; // 2s
  task_param_t p3 = {3,1500000000L,10000}; // 1.5s

  pthread_attr_t attr1, attr2, attr3;
  struct sched_param sched1, sched2, sched3;
  
  int ret1, ret2, ret3;

  pthread_attr_init(&attr1);
  pthread_attr_init(&attr2);
  pthread_attr_init(&attr3);

  // Politique SCHED_RR
  pthread_attr_setschedpolicy(&attr1, SCHED_RR);
  pthread_attr_setschedpolicy(&attr2, SCHED_RR);
  pthread_attr_setschedpolicy(&attr3, SCHED_RR);
  pthread_attr_setinheritsched(&attr1,PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setinheritsched(&attr2,PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setinheritsched(&attr3,PTHREAD_EXPLICIT_SCHED);
  
  // CONFIG 1 : Même priorité 
  //sched1.sched_priority = 10;
  //sched2.sched_priority = 10;
  
  // CONFIG 2 : T1 prio
  //sched1.sched_priority = 60;
  //sched2.sched_priority = 40;
  
  // CONFIG 3 : T1 > T3 > T2
  sched1.sched_priority = 80;
  sched2.sched_priority = 20;
  sched3.sched_priority = 50;
  
  pthread_attr_setschedparam(&attr1, &sched1);
  pthread_attr_setschedparam(&attr2, &sched2);
  pthread_attr_setschedparam(&attr3, &sched3);
  
  pthread_barrier_init(&barrier, NULL, 4); // T1 + T2 + T3 + main = 4
  pthread_mutex_init(&mutex, NULL);
  
  // Priority Inheritance
  /*
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
  
  pthread_mutex_init(&mutex, &attr);
  */
  ret1 = pthread_create(&t1,&attr1,task_function,&p1);
  ret2 = pthread_create(&t2,&attr2,task_function,&p2);
  ret3 = pthread_create(&t3,&attr3,task_function,&p3);
  
  if (ret1 == -1) {
    perror("pthread create t1");
    exit(1);
  }
  
  if (ret2 == -1) {
    perror("pthread create t2");
    exit(1);
  }  
  if (ret3 == -1) {
    perror("pthread create t3");
    exit(1);
  }  
  
  // Synchronisation
  pthread_barrier_wait(&barrier);
  
  pthread_join(t1,NULL);
  pthread_join(t2,NULL);
  pthread_join(t3,NULL);

  return 0;
}
