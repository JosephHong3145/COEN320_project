#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

char const *datasetPath = "dataset.csv";

// Variable-Of-Interest Position
enum VOIPosition {
  FuelConsumption = 1,
  EngineSpeed = 13,
  EngineCoolantTemp = 18,
  CurrentGear = 34,
  VehicleSpeed = 44,
};

int const VOIIndexes[] = {
  FuelConsumption,
  EngineSpeed,
  EngineCoolantTemp,
  CurrentGear,
  VehicleSpeed
};

char const *VOIFilePaths[] = {
  "fuelConsumption.csv",
  "engineSpeed.csv",
  "engineCoolantTemp.csv",
  "currentGear.csv",
  "vehicleSpeed.csv"
};

int const VOICount = sizeof(VOIFilePaths) / sizeof(*VOIFilePaths);

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

int main() {
  processDataset(datasetPath);
  return 0;
}