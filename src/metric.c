/*
 *
 * Student Name: Damiano Salvaterra
 *
 */

#include "metric.h"



/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/


float etx_est_rssi(uint16_t rssi){
  if(rssi > RSSI_HIGH_REF) return 1.0f;
  if(rssi < RSSI_LOW_THR) return 10.0f;
  //linear interpolation between RSSI_HIGH_REF and RSSI_THR
  float span   = (float)(RSSI_HIGH_REF - RSSI_LOW_THR);   /* > 0          */
  float offset = (float)(RSSI_HIGH_REF - rssi);       /* 0 ... span     */
  float frac   = offset / span;                       /* 0 ... 1        */

  /* 1  +  frac·9   ->  go from 1 (RSSI_HIGH_REF) ot 10 (RSSI_LOW_THR)      */
  return 1.0f + frac * 9.0f;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

float etx_update(uint16_t num_tx, uint16_t num_ack, float o_etx, uint16_t rssi){
  float n_etx;
  //if the entry has no acks yet, or no EWMA filtering is applied, default to rssi-based estimation
  if((num_ack == 0) || (ALPHA == 1)) n_etx = etx_est_rssi(rssi);
  else{
    n_etx = ((float)num_tx) / (float)num_ack;
    // EWMA filtering
    n_etx = ALPHA * o_etx + (1.0f - ALPHA) * n_etx;
  }
  return n_etx;
}
