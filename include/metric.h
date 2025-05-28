/*
 *
 * Student Name: Damiano Salvaterra
 *
 */

#ifndef METRIC_UTILS_H
#define METRIC_UTILS_H

#include <stdbool.h>
#include <float.h>
#include "contiki.h"
#include "rp_types.h"

#define RSSI_HIGH_REF (-35)
#define RSSI_LOW_THR (-85)
#define DELTA_ETX_MIN   0.30f 
#define THR_H       100.0f

#if RDC_MODE == RDC_NULLRDC
    #define ALPHA 0.9f //metric inertia
#elif RDC_MODE == RDC_CONTIKIMAC
    #define ALPHA 0.9f //metric inertia
#endif

/*-----METRIC DEFINITIONS-----*/
#define METRIC_Q_FRAC_BITS  4
#define METRIC_FP_SCALE     (1u << METRIC_Q_FRAC_BITS)   /* 16 -> Q12.4 */


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* Float -> Q12.4 -------------------------------------------------*/
static inline metric_q124_t metric_float_to_q124(float m){
  const float max_val = (float)(0xFFFFu) / METRIC_FP_SCALE; /* 4095.9375 */
  if(m >= max_val) return 0xFFFFu;
  return (metric_q124_t)(m * METRIC_FP_SCALE + 0.5f);
 }
/*---------------------------------------------------------------------------*/

static inline float metric_q124_to_float(metric_q124_t q){
   return (float)q / METRIC_FP_SCALE;
 }
/*---------------------------------------------------------------------------*/
static inline float metric_improv_thr(float cur_metric) {
  if(cur_metric <= 0.0f) return FLT_MAX; // deactivate improvement: something is wrong
  float thr = THR_H / cur_metric;
  /* dynamic thresholding: with larger metrics even small changes are privileged */
  return (thr < DELTA_ETX_MIN) ? DELTA_ETX_MIN : thr;
}
/*---------------------------------------------------------------------------*/
static inline bool preferred(float new_m, float cur_m){
  float thr = metric_improv_thr(cur_m);
  return (new_m + thr) < cur_m;
}
/*---------------------------------------------------------------------------*/

static inline float metric(float adv_metric, float etx){
  return adv_metric + etx;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/


float etx_est_rssi(uint16_t rssi); 


float etx_update(uint16_t num_tx, uint16_t num_ack, float o_etx, uint16_t rssi);
#endif /* METRIC_UTILS_H */
