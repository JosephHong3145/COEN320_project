#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>

#include "main.h"

#define VOICount (sizeof(VOIFilePaths) / sizeof(*VOIFilePaths))
double sharedMemory[VOICount] = {0};
ProducerThreadArguments producerThreadArgs[VOICount];
sigset_t sigset;

// In nanoseconds...
uint64_t const producerOffsets[VOICount] = {1, 2, 3, 4, 5};
// In seconds...
uint64_t const producerPeriods[VOICount] = {
  5, // Fuel Consumption
  3, // Engine Speed
  4, // Engine Coolant Temp
  7, // Current Gear
  2  // Vehicle Speed
};
uint64_t const consumerOffset = 100;
uint64_t const consumerPeriod = 5;
sem_t consumerWaitSem;

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
  int max = -1;
  int accumulators[VOICount] = {0};
  while(fgets(line, sizeof(line), datasetFile) != NULL) {
    for(int i = 0; i < VOICount; i += 1) {
      if(accumulators[i] == 0) {
        fprintf(VOIFiles[i], "%lf\n", scanColumn(line, VOIIndexes[i]));
        accumulators[i] += producerPeriods[i];
      }
      accumulators[i]--;
    }
    if(--max == 0) break;
  }

  for(int i = 0; i < VOICount; i += 1) fclose(VOIFiles[i]);
  fclose(datasetFile);
}

void maskThreadFromHandlingAlarmSignal(void) {
	sigset_t mask;
	sigemptyset(&mask); 
  sigaddset(&mask, SIGALRM);               
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void *producerThread(void *data) {
  maskThreadFromHandlingAlarmSignal();
  ProducerThreadArguments *arguments = (ProducerThreadArguments*) data;
  printf("Producer thread for VOI %s started.\n", arguments->label);
  FILE *file = fopen(arguments->filePath, "r");
  char line[256];
  uint64_t start;
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  start = tv.tv_sec * THOUSAND + tv.tv_nsec / MILLION;
  while(fgets(line, sizeof(line), file) != NULL) {
    sem_wait(&arguments->waitSem);
    clock_gettime(CLOCK_MONOTONIC, &tv);
    uint64_t current = tv.tv_sec * THOUSAND + tv.tv_nsec / MILLION;
    printf("//// PRODUCER THREAD %s WAKING UP (%ld) /////\n", arguments->label, current - start);
    double value = 0;
    sscanf(line, "%lf\n", &value);
    *(arguments->sharedMemoryPtr) = value;
  }
  *(arguments->sharedMemoryPtr) = VOI_NIL;
  printf("Producer thread for VOI %s done.\n", arguments->label);
  fclose(file);
  return NULL;
}

void *consumerThread(void *data) {
  maskThreadFromHandlingAlarmSignal();
  printf("Consumer thread started.\n");
  uint64_t start;
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  start = tv.tv_sec * THOUSAND + tv.tv_nsec / MILLION;
  while(true) {
    // Waiting code...
    sem_wait(&consumerWaitSem);
    clock_gettime(CLOCK_MONOTONIC, &tv);
    uint64_t current = tv.tv_sec * THOUSAND + tv.tv_nsec / MILLION;
    printf("//// CONSUMER THREAD UNLOCKED (%ld) ////\n", current - start);
    int threadsDone = 0;
    for(int i = 0; i < VOICount; i += 1) {
      if(sharedMemory[i] != VOI_NIL) {
        printf("%s: %lf\n", VOILabels[i], sharedMemory[i]);
      } else {
        threadsDone++;
      }
    }
    printf("Threads done: %d/%ld\n", threadsDone, VOICount);
    if(threadsDone == VOICount) break;
  }
  printf("Consumer thread done.\n");
  return NULL;
}

void createTimer(uint64_t offset, int period, int identifier) {
  printf("Create timer: %ld, %d, %d\n", offset, period, identifier);
  struct itimerspec specs;
  specs.it_value.tv_sec = offset / MILLION;
	specs.it_value.tv_nsec = (offset % MILLION) * THOUSAND;
	specs.it_interval.tv_sec = period / MILLION;
	specs.it_interval.tv_nsec = (period % MILLION) * THOUSAND;
  struct sigevent sigev;
	memset(&sigev, 0, sizeof(struct sigevent));
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = SIGALRM;
	sigev.sigev_value.sival_int = identifier;
  timer_t timer;
  int res = timer_create(CLOCK_MONOTONIC, &sigev, &timer);
  if (res < 0) {
		perror("Timer Create");
		exit(-1);
	}
  res = timer_settime(timer, 0, &specs, NULL);
  if (res < 0) {
		perror("Timer Launch");
		exit(-1);
	}
}

void *timerThread(void *arg) {
  sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);
  for(int i = 0; i < VOICount; i += 1) {
    createTimer(producerOffsets[i], producerPeriods[i] * MILLION, i);
  }
  createTimer(consumerOffset, consumerPeriod * MILLION, -1);
  while(1) {
    siginfo_t info;
    sigwaitinfo(&sigset, &info);
    switch(info.si_value.sival_int) {
      case -1:
        sem_post(&consumerWaitSem);
        break;
      default:
        sem_post(&producerThreadArgs[info.si_value.sival_int].waitSem);
        break;
    }
  }
}

int main() {
  processDataset(datasetPath);
  pthread_t producerThreadIDs[VOICount];
  pthread_t consumerThreadID;
  pthread_t timerThreadID;
  maskThreadFromHandlingAlarmSignal();
  sem_init(&consumerWaitSem, 0, 0);
  pthread_create(&consumerThreadID, NULL, &consumerThread, NULL);
  for(int i = 0; i < VOICount; i += 1) {
    producerThreadArgs[i].label = VOILabels[i];
    producerThreadArgs[i].filePath = VOIFilePaths[i];
    producerThreadArgs[i].sharedMemoryPtr = &sharedMemory[i];
    sem_init(&producerThreadArgs[i].waitSem, 0, 0);
    pthread_create(&producerThreadIDs[i], NULL, &producerThread, &producerThreadArgs[i]);
  }
  pthread_create(&timerThreadID, NULL, &timerThread, NULL);
  for(int i = 0; i < VOICount; i += 1) {
    pthread_join(producerThreadIDs[i], NULL);
  }
  pthread_join(consumerThreadID, NULL);
  return 0;
}