/*
 *
 * Student Name: Damiano Salvaterra
 *
 */

#ifndef NBR_TBL_H_UT
#define NBR_TBL_H_UT

#include "rp_types.h"
#include "metric.h"
#include <stdbool.h>


/*----Entry types----*/
#define NODE_PARENT      0
#define NODE_CHILD       1
#define NODE_DESCENDANT  2
#define NODE_NEIGHBOR    3

typedef struct{
    uint8_t type;
    clock_time_t age;
    linkaddr_t nexthop;
    uint8_t hops;
    float etx; //etx of the link
    uint16_t num_tx;
    uint16_t num_ack;
    metric_q124_t adv_metric; //advertised metric from this node
} entry_t;


#define ENTRY_EXPIRATION_TIME (60 * CLOCK_SECOND)

#define VALID(age) ((clock_time() - (age)) < ENTRY_EXPIRATION_TIME)
#define ALWAYS_VALID_AGE 0xFFFFFFFF
#define ALWAYS_INVALID_AGE 0


void nbr_tbl_lookup(nbr_table_t* nbr_tbl, linkaddr_t* nexthop, const linkaddr_t* dst_addr, const linkaddr_t* parent);

/*refresh entry in the neighbor table*/
static inline void nbr_tbl_refresh(nbr_table_t* nbr_tbl, const linkaddr_t* addr){
  entry_t *entry = (entry_t *) nbr_table_get_from_lladdr(nbr_tbl, addr);
  if(entry != NULL) {
    entry->age = clock_time();
  }
}

void nbr_tbl_update(nbr_table_t* nbr_tbl,struct rp_conn* conn, const linkaddr_t* tx_addr, tpl_vec_t net_buf);

void remove_subtree(nbr_table_t* nbr_tbl,struct rp_conn* conn, linkaddr_t ch_addr);

void nbr_tbl_cleanup_cb(void *ptr); 


#endif /* NBR_TBL_H_UT */
