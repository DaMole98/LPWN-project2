#ifndef __RP__
#define __RP__
/*---------------------------------------------------------------------------*/

#include "rp_types.h"
#include "metric.h"
#include "nbr_tbl_utils.h"

/*---------------------------------------------------------------------------*/

/* Callback structure (for the application) */
struct rp_callbacks {
  /* The recv function should be called when a node receives an any-to-any
   * packet (i.e., a packet sent by the application using rp_send function).
   *
   * Arguments: 
   * const linkaddr_t *src: source of any-to-any message
   * uint8_t hops: number of hops from source to final destination
   */
  void (* recv)(const linkaddr_t *src, uint8_t hops);
};


/*-------------------------------CONSTANTS AND TIMERS------------------------------------*/
/* Constants definition. Some constant are defined at compile time depending on the 
    RDC_MODE macro. This macro is found in project-conf.h
*/
#define MAX_PATH_LENGTH 40 //for the testbed we have 36 nodes

#define TREE_BEACON_INTERVAL ((clock_time_t)(60.0f * (float)CLOCK_SECOND))

#define SUBTREE_REPORT_OFFSET ((float)TREE_BEACON_INTERVAL / 3.0f)

#define NBR_TBL_CLEANUP_INTERVAL ((clock_time_t)(15.0f * (float)CLOCK_SECOND))


/* -----constants for NullRDC-----*/
#if RDC_MODE == RDC_NULLRDC

    #define TREE_BEACON_FORWARD_DELAY ((clock_time_t)(((float)CLOCK_SECOND / 10.0f) + ((float)(random_rand() % (CLOCK_SECOND / 8)))))

    #define SUBTREE_REPORT_BASE_DEL(hops) (((clock_time_t)(((float)(5*CLOCK_SECOND)) / (float)(hops))) + (float)(4*(random_rand() % CLOCK_SECOND/10)))

    #define SUBTREE_REPORT_NODE_INTERVAL(hops) ((clock_time_t)(SUBTREE_REPORT_OFFSET * (1.0f + (1.0f / (float)(hops)))))

    #define SUBTREE_REPORT_DELAY ((clock_time_t)(((float)CLOCK_SECOND / 10.0f) + (float)(random_rand() % (CLOCK_SECOND / 10))))


/* -----constants for ContikiMAC-----*/
#elif RDC_MODE == RDC_CONTIKIMAC

    #define TREE_BEACON_FORWARD_DELAY ((clock_time_t)(((float)CLOCK_SECOND / 8.0f) + ((float)(8*(random_rand() % CHANNEL_CHECK_INTERVAL_TICKS)))))

    #define SUBTREE_REPORT_BASE_DEL(hops) (((clock_time_t)((float)5*CLOCK_SECOND) / (float)(hops) ) + (float)(4*(random_rand() % CHANNEL_CHECK_INTERVAL_TICKS)))\

    #define SUBTREE_REPORT_NODE_INTERVAL(hops) ((clock_time_t)( SUBTREE_REPORT_OFFSET * (1.0f + (1.0f / (float)(hops))) ))

    #define SUBTREE_REPORT_DELAY ((clock_time_t)(((float)CLOCK_SECOND / 10.0f) + (float)(4*(random_rand() % CHANNEL_CHECK_INTERVAL_TICKS))))

#endif


/*-----UNICAST HEADER DEFINITIONS-----*/

#define UC_TYPE_DATA 0
#define UC_TYPE_REPORT 1


struct uc_hdr{
    uint8_t type;
    linkaddr_t s_addr;
    linkaddr_t d_addr;
    uint8_t hops;
}__attribute__((packed));


/*-----BROADCAST MESSAGE DEFINITION-----*/
struct bc_msg{
    uint16_t seqn;
    metric_q124_t metric_q124; //Q12.4 encoding to reduce float to 2 bytes
    uint8_t hops;
    linkaddr_t parent;
  }__attribute__((packed));




/*---------------------------------------------------------------------------*/
/* === Payload budgeting =================== */
/*---------------------------------------------------------------------------*/
#ifndef PACKETBUF_SIZE
#error "PACKETBUF_SIZE must be defined by <contiki-conf.h>"
#endif
#ifndef PACKETBUF_HDR_SIZE
//#error "PACKETBUF_HDR_SIZE must be defined by platform headers"
#define PACKETBUF_HDR_SIZE 9 //manual fallback, works for zolertia firefly and tmote sky
#endif

#define RP_TPL_META_LEN      1                       /* size field      */
#define RP_TPL_UC_HDR_LEN    6   /* unicast header byte length          */

#define RP_TPL_MAX_BYTES (PACKETBUF_SIZE - PACKETBUF_HDR_SIZE - RP_TPL_UC_HDR_LEN - RP_TPL_META_LEN)

#define RP_MAX_STAT_PER_FRAG (RP_TPL_MAX_BYTES / 3) /*3: sizeof/(stat_addr_t)*/

#if RP_MAX_STAT_PER_FRAG < 1
#error "stat_addr_t does not fit into PACKETBUF_SIZE"
#endif


/*---------------------------------------------------------------------------*/
    /*---------------------------------------------------------------------------*/
/* Initialize a routing protocol connection 
 *  - conn -- a pointer to a connection object 
 *  - channels -- starting channel C
 *  - sink -- initialize in either sink or router mode
 *  - callbacks -- a pointer to the callback structure
 */
void rp_open(
    struct rp_conn* conn, 
    uint16_t channels, 
    bool sink,
    const struct rp_callbacks *callbacks);
/*---------------------------------------------------------------------------*/
/* Send packet to a given destination dest 
 * Arguments: 
 * struct rp_conn *c: a pointer to a connection object 
 * const linkaddr_t *dest: the final link layer destination address to send the
 *                         the message to.
 * Return value:
 * Non-zero if the packet could be sent, zero otherwise
 */
int rp_send(struct rp_conn *c, const linkaddr_t *dest);
/*---------------------------------------------------------------------------*/
extern void subtree_report_cb(void* ptr);

void change_parent(void *ptr);

_Static_assert((MAX_PATH_LENGTH * 10) <= ((1 << 12) - 1),
               "Q12.4 overflow: increase integer bits or reduce MAX_PATH_LENGTH");

#endif /* __RP__ */