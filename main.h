// Variable-Of-Interest Position
enum VOIPosition {
  FuelConsumption = 1,
  EngineSpeed = 13,
  EngineCoolantTemp = 18,
  CurrentGear = 34,
  VehicleSpeed = 44,
};

char const *datasetPath = "dataset.csv";

int const VOIIndexes[] = {
  FuelConsumption,
  EngineSpeed,
  EngineCoolantTemp,
  CurrentGear,
  VehicleSpeed
};

char const *VOILabels[] = {
  "Fuel Consumption",
  "Engine Speed",
  "Engine Coolant Temp",
  "Current Gear",
  "Vehicle Speed"
};

char const *VOIFilePaths[] = {
  "fuelConsumption.csv",
  "engineSpeed.csv",
  "engineCoolantTemp.csv",
  "currentGear.csv",
  "vehicleSpeed.csv"
};

typedef struct ProducerThreadArguments {
    char const *label;
    char const *filePath;
    double *sharedMemoryPtr;
    pthread_cond_t waitCond;
    pthread_mutex_t lock;
} ProducerThreadArguments;

#define VOI_NIL -1337

const uint64_t MILLION = 1000000;
const uint64_t THOUSAND = 1000;