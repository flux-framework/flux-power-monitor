#ifndef FLUX_POWER_POLICY_H
#define FLUX_POWER_POLICY_H
typedef enum {
  GPU_FOCUSED,
  UTILIZATION_AWARE,
  CURRENT_POWER,
  FAIRNESS
} POWER_POLICY_TYPE;
#endif
