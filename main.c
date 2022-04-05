#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>

#include "main.h"

#define VOICount (sizeof(VOIFilePaths) / sizeof(*VOIFilePaths))
double sharedMemory[VOICount];
ProducerThreadArguments producerThreadArgs[VOICount];
sem_t producerMutex;
sigset_t sigst;

void assertFatal(bool condition, char const *format, ...) {
  if(condition == true) return;
  va_list arguments;
  va_start(arguments, format);
  vprintf(format, arguments);
  va_end(arguments);
  exit(EXIT_FAILURE);
}

double scanColumn(char *line, int columnPosition) {
  double value;
  char scanStr[1024];
  scanStr[0] = 0;
  for(int i = 0; i < columnPosition - 1; i += 1) strcat(scanStr, "%*lf,");
  strcat(scanStr, "%lf");
  sscanf(line, scanStr, &value);
  return value;
}

void processDataset(char const *datasetPath) {
  // Open dataset file for reading
  FILE *datasetFile = fopen(datasetPath, "r");
  assertFatal(datasetFile != NULL, "Could not open file (%s)\n", datasetPath);
  FILE *VOIFiles[VOICount];
  for(int i = 0; i < VOICount; i += 1) {
    // Open variable-of-interest files for writing
    // Providing the "w" option overwrites the file
    VOIFiles[i] = fopen(VOIFilePaths[i], "w");
    assertFatal(VOIFiles[i]  != NULL, "Could not open file (%s)\n", VOIFilePaths[i]);
  }

  char line[2048];
  line[0] = 0;
  // Skip first line
  fgets(line, sizeof(line), datasetFile);
  while(fgets(line, sizeof(line), datasetFile) != NULL) {
    for(int i = 0; i < VOICount; i += 1) 
      fprintf(VOIFiles[i], "%lf\n", scanColumn(line, VOIIndexes[i]));
  }

  for(int i = 0; i < VOICount; i += 1) fclose(VOIFiles[i]);
  fclose(datasetFile);
}

// https://moodle.concordia.ca/moodle/pluginfile.php/5349073/mod_resource/content/2/timers.c
#define ONE_THOUSAND 1000
#define ONE_MILLION	1000000
int start_periodic_timer(uint64_t offset, int period) {
	struct itimerspec timer_spec;
	struct sigevent sigev;
	timer_t timer;
	const int signal = SIGALRM;
	int res;
	
	/* set timer parameters */
	timer_spec.it_value.tv_sec = offset / ONE_MILLION;
	timer_spec.it_value.tv_nsec = (offset % ONE_MILLION) * ONE_THOUSAND;
	timer_spec.it_interval.tv_sec = period / ONE_MILLION;
	timer_spec.it_interval.tv_nsec = (period % ONE_MILLION) * ONE_THOUSAND;
	
	sigemptyset(&sigst); // initialize a signal set
	sigaddset(&sigst, signal); // add SIGALRM to the signal set
	sigprocmask(SIG_BLOCK, &sigst, NULL); //block the signal
	
	/* set the signal event a timer expiration */
	memset(&sigev, 0, sizeof(struct sigevent));
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = signal;
	
	/* create timer */
	res = timer_create(CLOCK_MONOTONIC, &sigev, &timer);
	
	if (res < 0) {
		perror("Timer Create");
		exit(-1);
	}
	
	/* activate the timer */
	return timer_settime(timer, 0, &timer_spec, NULL);
}

void *producerThread(void *data) {
  ProducerThreadArguments *arguments = (ProducerThreadArguments*) data;
  printf("Producer thread for VOI %s started.\n", arguments->label);
  FILE *file = fopen(arguments->filePath, "r");
  char line[256];
  while(fgets(line, sizeof(line), file) != NULL) {
    double value = 0;
    sscanf(line, "%lf\n", &value);
    *(arguments->sharedMemoryPtr) = value;
    sem_post(&producerMutex);
    int dummy;
    sigwait(&sigst, &dummy);
    printf("//// PRODUCER THREAD %s WAKING UP /////\n", arguments->label);
  }
  *(arguments->sharedMemoryPtr) = VOI_NIL;
  printf("Producer thread for VOI %s done.\n", arguments->label);
  sem_post(&producerMutex);
  fclose(file);
  return NULL;
}

void *consumerThread(void *data) {
  int running = true;
  printf("Consumer thread started.\n");
  while(running) {
    for(int i = 0; i < VOICount; i += 1) {
      sem_wait(&producerMutex);
    }
    printf("//// CONSUMER THREAD UNLOCKED ////\n");
    for(int i = 0; i < VOICount; i += 1) {
      if(sharedMemory[i] == VOI_NIL) {
        running = false;
      } else {
        printf("%s: %lf\n", VOILabels[i], sharedMemory[i]);
      }
    }
  }
  printf("Consumer thread done.\n");
  return NULL;
}

int main() {
  processDataset(datasetPath);
  sem_init(&producerMutex, 0, 1);
  pthread_t producerThreadIDs[VOICount];
  pthread_t consumerThreadID;
  pthread_create(&consumerThreadID, NULL, &consumerThread, NULL);
  for(int i = 0; i < VOICount; i += 1) {
    producerThreadArgs[i].label = VOILabels[i];
    producerThreadArgs[i].filePath = VOIFilePaths[i];
    producerThreadArgs[i].sharedMemoryPtr = &sharedMemory[i];
    pthread_create(&producerThreadIDs[i], NULL, &producerThread, &producerThreadArgs[i]);
  }
  int res = start_periodic_timer(0, 5000000);
	if (res < 0){
		perror("Start periodic timer");
		return -1;
	}
  for(int i = 0; i < VOICount; i += 1) {
    pthread_join(producerThreadIDs[i], NULL);
  }
  pthread_join(consumerThreadID, NULL);
  return 0;
}
