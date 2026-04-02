#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>

#define SOCKET_PORT 10020
#define SOCKET_SERVER "127.0.0.1"

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
sem_t sem;
int fd;

// =========================
//  TACHE PERIODIQUE
// =========================

typedef struct {
  int id;
  long period_ns;
} task_param_t;

void check_battery(){
  char buffer[256];
  send (fd,"B\n", strlen("B\n"),0);
  int n = recv(fd, buffer, 256, 0);
  printf("Battery level : %d\n",n);
}

void drive_towards_battery_station(){
	// Pour plus tard
}

void drive_robot(){
  double left_sensor, right_sensor;
  char buffer[256];
  int angle = 0;

  send (fd,"S\n", strlen("S\n"),0);
  int n = recv(fd,buffer, 256, 0);
  buffer[n]= '\0';
  printf("Buffer : %s\n", buffer);
  sscanf(buffer,"S,%lf,%lf", &left_sensor,&right_sensor);
  //printf("Distance sensor 0 value: %.3f\n", left_sensor);
  //printf("Distance sensor 1 value: %.3f\n", right_sensor);
  if ((left_sensor > 20)||(right_sensor > 20)){
	float random = ((float)rand() / (float) (RAND_MAX)) * 3.14;
	int sign = rand() % (2);
	printf("Valeur de sign : %d \n", sign);
	if (sign){
		random = -random;	
	}
	char str[7];
	sprintf(str, "T,%f\n", random);
	printf("Valeur de str : %s",str);
	send(fd,str,strlen("T,3.14\n"),0);
	n = recv(fd,buffer,256,0);

  } else {
	printf("JE DOIS AVANCER ! \n");
    send(fd,"M,50,50\n",strlen("M,50,50\n"),0);
  }
}

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
    if (param->id == 1) { // On est dans la première tâche (Drive Robot from wall to wall)
      pthread_mutex_lock(&mutex);
      drive_robot();
      pthread_mutex_unlock(&mutex);
    } else if (param->id == 2){ // On est dans la deuxième tâche (Display on console the batter level)
      pthread_mutex_lock(&mutex);
      check_battery();
      pthread_mutex_unlock(&mutex);
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
  // ---------------------------
  // PARTIE RESEAU
  // ---------------------------
  struct sockaddr_in address;
  const struct hostent *server;
  int rc;
  // Création du socket
  fd = socket(AF_INET,SOCK_STREAM, 0);
  if (fd == -1){
    printf("cannot create socket \n");
    exit(1);
  }
  // Configurer les adresses 
  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_port = htons(SOCKET_PORT);
  server = gethostbyname(SOCKET_SERVER);
  
  if (server){
    memcpy((char *)&address.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
  } else {
    printf("cannot resolve server name \n");
    close(fd);
    exit(1);
  }
  // Se connecter au serveur
  rc = connect(fd,(struct sockaddr *)&address, sizeof(struct sockaddr));
  if (rc == -1) {
    printf("cannot connect to server \n");
    close(fd);
    exit(1);
  }
  
  fflush(stdout);

  pthread_t t1, t2;
  task_param_t p1 = {1,250000000L}; // 0.25s
  task_param_t p2 = {2,2000000000L}; // 2s
  
  pthread_attr_t attr1, attr2;
  struct sched_param sched1, sched2;
  int ret1, ret2;
  
  pthread_attr_init(&attr1);
  pthread_attr_init(&attr2);
  
  // Politique SCHED_RR
  pthread_attr_setschedpolicy(&attr1, SCHED_RR);
  pthread_attr_setschedpolicy(&attr2, SCHED_RR);
  pthread_attr_setinheritsched(&attr1,PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setinheritsched(&attr2,PTHREAD_EXPLICIT_SCHED);
  
  sched1.sched_priority = 80;
  sched2.sched_priority = 10;
  
  pthread_attr_setschedparam(&attr1, &sched1);
  pthread_attr_setschedparam(&attr2, &sched2);
  
  pthread_barrier_init(&barrier, NULL, 3); // T1 + T2 + main
  pthread_mutex_init(&mutex, NULL);
  
  // Pas besoin de priority inheritance pour l'instant
  
  ret1 = pthread_create(&t1,&attr1,task_function,&p1);
  ret2 = pthread_create(&t2,&attr2,task_function,&p2);
  
  if (ret1 == -1) {
    perror("pthread create t1");
    exit(1);
  }
  
  if (ret2 == -1) {
    perror("pthread create t2");
    exit(1);
  } 
  
  // Synchronisation
  pthread_barrier_wait(&barrier);
  
  pthread_join(t1,NULL);
  pthread_join(t2,NULL);
  
  close(fd);
  return 0;
}


















