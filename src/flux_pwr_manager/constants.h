#ifndef FLUX_PWR_MANAGER_CONSTANTS_H
#define FLUX_PWR_MANAGER_CONSTANTS_H
//Power Monitor
#define INTERVAL_SEC 0           // Seconds part of the interval
#define INTERVAL_NSEC 500000000L // Nanoseconds part of the interval (500 ms)
//FFT 
#define SAMPLING_RATE 2 // SAMPLING RATE IN A SECOND
#define THIRTY_SECONDS (30 * SAMPLING_RATE)
#define THREE_MINUTES (180 * SAMPLING_RATE)
//NODE Power INFO
#define MAX_NODE_PWR 3050
#define MIN_NODE_PWR 1000

#endif
