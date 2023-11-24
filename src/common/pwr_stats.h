#ifndef FLUX_PWR_MANAGER_POWER_STATS_H
#define FLUX_PWR_MANAGER_POWER_STATS_H
typedef struct{
 double max_pwr;
  double min_pwr;
  double avg_pwr;
  double median_pwr;
  double duration_perc;
  double powerlimit;
}pwr_stats_t;
#endif
