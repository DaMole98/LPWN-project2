#ifndef RP_TYPES
#define RP_TYPES

#include "contiki.h"
#include <stdbool.h>
#include <float.h>
#include "net/rime/rime.h"
#include "net/netstack.h"
#include "core/net/linkaddr.h"
#include "net/packetbuf.h"
#include "net/nbr-table.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "lib/list.h"
#include "lib/memb.h"
#include <stdio.h> /* For printf */

typedef uint16_t metric_q124_t;      /* Q12.4: 12-bit int, 4-bit frac */
#define METRIC_Q_FRAC_BITS   4
#define METRIC_Q_SCALE       (1u << METRIC_Q_FRAC_BITS)   /* 16 */

#define METRIC_Q124_INF  ((metric_q124_t)0xFFFFu)   /* 4095.9375 approx */


/*----Struct for collecting the routing table changes (to send in topology reports)----*/
#define STATUS_ADD 1
#define STATUS_REMOVE 0
//struct with the information of the node to be added/removed
typedef struct __attribute__((packed)){
    linkaddr_t addr;
    uint8_t status;
} stat_addr_t;

//vector of addresses to be added/removed
typedef struct __attribute__((packed)){
    uint8_t size;
    stat_addr_t stat_addr_arr[NBR_TABLE_CONF_MAX_NEIGHBORS]; 
} tpl_vec_t;


//args struct for the cleanup callback
typedef struct{
    struct rp_conn* conn;
    nbr_table_t* nbr_tbl;
} cb_args_t;

/*---------------------------------------------------------------------------*/
/* Structure of the connection. This struct holds the state of the protocol:
    everything related to the routing protocol is in here */
struct rp_conn {
    struct broadcast_conn bc;
    struct unicast_conn uc;
    uint16_t seqn;
    const struct rp_callbacks* callbacks;
    linkaddr_t parent; //parent node
    struct ctimer beacon_timer; //timer for sending beacons
    struct ctimer nbr_tbl_cleanup_timer; //timer for routing table cleanup
    cb_args_t clu_args;

    metric_q124_t metric; //metric to the sink
    uint8_t hops; //number of hops to the sink

    bool sink; //true if the node is the sink
    tpl_vec_t tpl_buf; //vector of topology changes
    uint8_t buf_off; //offset for the buffer, used when the buffer has to be fragmented in multiple packets
    linkaddr_t last_uc_daddr; //last unicast destination address. Used to refresh in case of ack and to trigger parent change;
  };



#endif /*RP_TYPES*/