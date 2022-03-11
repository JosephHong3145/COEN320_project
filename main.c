#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#include "main.h"

#define VOICount (sizeof(VOIFilePaths) / sizeof(*VOIFilePaths))
double sharedMemory[VOICount];
ProducerThreadArguments producerThreadArgs[VOICount];
sem_t producerMutex;

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
  int max = 5;
  while(fgets(line, sizeof(line), datasetFile) != NULL) {
    for(int i = 0; i < VOICount; i += 1) 
      fprintf(VOIFiles[i], "%lf\n", scanColumn(line, VOIIndexes[i]));
    if(--max == 0) break;
  }

  for(int i = 0; i < VOICount; i += 1) fclose(VOIFiles[i]);
  fclose(datasetFile);
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
    sleep(5);
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
  for(int i = 0; i < VOICount; i += 1) {
    pthread_join(producerThreadIDs[i], NULL);
  }
  pthread_join(consumerThreadID, NULL);
  return 0;
}