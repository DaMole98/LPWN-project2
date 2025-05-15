/*
 *
 * Student Name: Damiano Salvaterra
 *
 */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#include "rp.h"
#include "metric.h"
/*---------------------------------------------------------------------------*/

NBR_TABLE(entry_t, nbr_tbl); //nbr table registration

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#if USR_DEBUG == 1
/*____________________________USR_DEBUG____________________________*/

static void rp_print_routing_table(struct rp_conn *conn); 
static void print_topology_report(const linkaddr_t* child_addr); 
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#endif
  /*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*----------------------------------TIMERS-----------------------------------*/
static struct ctimer subtree_report_timer; //timer for topology reports
static struct ctimer nbr_tbl_cleanup_timer;
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*-----------------------FUNCTION DECLARATIONS-----------------------*/
/*Callbacks declarations*/
static void bc_recv(struct broadcast_conn *b_conn, const linkaddr_t *tx_addr);
static void uc_recv(struct unicast_conn *u_conn, const linkaddr_t *from);
static void uc_sent(struct unicast_conn* c, int status, int num_tx);
static void beacon_timer_cb(void* ptr);

/*Initialize Rime Callback structs*/
struct broadcast_callbacks bc_cb = {.recv = bc_recv, .sent = NULL};
struct unicast_callbacks uc_cb = {.recv = uc_recv, .sent = uc_sent};

//Topology maintenance functions
static void reset_connection_status(struct rp_conn* conn, uint16_t seqn, bool sink);
static inline void flush_tpl_buf(struct rp_conn* conn);


/*---------------------------------------------------------------------------*/
/*------------------RP CONNECTION INITIALIZATION------------------*/

void rp_open(struct rp_conn* conn, uint16_t channels, bool sink, const struct rp_callbacks *callbacks)
{
  /*---INIT CONNECTION---*/
  linkaddr_copy(&conn->parent, &linkaddr_null); //init parent to null
  conn->metric = METRIC_Q124_INF;
  conn->seqn = 0;
  conn->sink = sink;
  conn->hops = 0xFF;
  conn->callbacks = callbacks;
  conn->tpl_buf.size = 0;
  //cleanup callback args
  conn->clu_args.conn = conn; conn->clu_args.nbr_tbl = nbr_tbl;
  /*---Open RIME primitives*/
  broadcast_open(&conn->bc, channels, &bc_cb);
  unicast_open(&conn->uc, channels+1, &uc_cb);
  

  if(conn->sink){
    conn->metric=0;
    conn->hops=0;
    ctimer_set(&conn->beacon_timer, CLOCK_SECOND, beacon_timer_cb, conn); // set the sink to send a beacon at the beginning
 
  }
  nbr_table_register(nbr_tbl, NULL);

  /* Schedule the first cleanup */ 
  ctimer_set(&nbr_tbl_cleanup_timer, NBR_TBL_CLEANUP_INTERVAL, nbr_tbl_cleanup_cb, &conn->clu_args);
  #if USR_DEBUG == 1
  printf("Node %02x:%02x is initializing rp connection\n",linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
  #endif
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
//flushes the report buffer
static inline void flush_tpl_buf(struct rp_conn* conn){
  conn->tpl_buf.size = 0;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
 /* Resets the connection status for.
  This function updates the neighbor table by setting the age of descendants
  to an invalid value, downgrading any children or parent nodes to neighbors,
  and then resets the local connection state. Then flushes the topology
  report buffer and performs a cleanup of the neighbor table. */

static void reset_connection_status(struct rp_conn* conn, uint16_t seqn, bool sink){
    entry_t* e = nbr_table_head(nbr_tbl);
    while(e != NULL){ 
        if(e->type == NODE_DESCENDANT) //set descendants to be removed from the neighbor table
            e->age = ALWAYS_INVALID_AGE;
        else if(e->type == NODE_CHILD || e->type == NODE_PARENT) //downgrade the parent and the childs to neighbors
            e->type = NODE_NEIGHBOR;
        e = nbr_table_next(nbr_tbl, e);
    }
    //local state reset
    linkaddr_copy(&conn->parent, &linkaddr_null);
    conn->metric = sink ? 0 :  METRIC_Q124_INF;
    conn->seqn = seqn;
    flush_tpl_buf(conn);
    ctimer_set(&nbr_tbl_cleanup_timer, NBR_TBL_CLEANUP_INTERVAL, nbr_tbl_cleanup_cb, &conn->clu_args);
    nbr_tbl_cleanup_cb(&conn->clu_args);
    
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*----------------------------Any-To-Any handling----------------------------*/


//called only by the application
int rp_send(struct rp_conn *conn, const linkaddr_t *dst_addr){

    linkaddr_t nexthop;
    nbr_tbl_lookup(nbr_tbl, &nexthop, dst_addr, &conn->parent);

    if(!conn->sink && linkaddr_cmp(&conn->parent, &linkaddr_null)) return -1; //if the node is not connected return an error
  
    if(packetbuf_hdralloc(sizeof(struct uc_hdr))){ //insert the header into the packet buffer
      struct uc_hdr hdr = {.s_addr=linkaddr_node_addr, .d_addr = *dst_addr, .hops=0, .type = UC_TYPE_DATA}; //init header
      memcpy(packetbuf_hdrptr(), &hdr, sizeof(hdr));
      #if USR_DEBUG == 1
      printf("[LOG] Node %02x:%02x is SENDING packet to %02x:%02x via next-hop %02x:%02x\n",
        linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
        dst_addr->u8[0], dst_addr->u8[1],
        nexthop.u8[0], nexthop.u8[1]);
      rp_print_routing_table(conn);
      #endif
      //keep track of the last unicast. Used for uc_recv for etx computation
      conn->last_uc_daddr = nexthop;
      return unicast_send(&conn->uc, &nexthop);
    }
    else return -2;
  }
    
  /*---------------------------------------------------------------------------*/
  //called when the data have to be forwarded
  static int forward_data(struct rp_conn* conn, struct uc_hdr hdr){
    if(packetbuf_hdralloc(sizeof(struct uc_hdr))) //restore the header into the packet buffer
      memcpy(packetbuf_hdrptr(), &hdr, sizeof(hdr));
  
    linkaddr_t nexthop;
    nbr_tbl_lookup(nbr_tbl, &nexthop, &hdr.d_addr, &conn->parent);
  
    #if USR_DEBUG == 1
    printf("[LOG] Node %02x:%02x is FORWARDING packet from %02x:%02x to destination %02x:%02x via next-hop %02x:%02x\n",
      linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
      hdr.s_addr.u8[0], hdr.s_addr.u8[1],
      hdr.d_addr.u8[0], hdr.d_addr.u8[1],
      nexthop.u8[0], nexthop.u8[1]);
    rp_print_routing_table(conn);
    #endif
    //keep track of the last unicast. Used for uc_recv for etx computation
    conn->last_uc_daddr = nexthop;
    return unicast_send(&conn->uc, &nexthop);
  }  
  

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*------------------------------BEACON HANDLING------------------------------*/

static void beacon_timer_cb(void* ptr){
    struct rp_conn* conn = (struct rp_conn*)ptr;
  
    /*SINK LOGIC*/
    if(conn->sink){
      conn->seqn++; 
      reset_connection_status(conn, conn->seqn, conn->sink); //start a new epooch
      ctimer_set(&conn->beacon_timer, TREE_BEACON_INTERVAL, beacon_timer_cb, conn); //schedule the next flood
    }

    /*send beacon*/
    packetbuf_clear();
    struct bc_msg msg = {.seqn = conn->seqn, .metric_q124 = conn->metric, .hops = conn->hops, .parent = conn->parent};
    memcpy(packetbuf_dataptr(), &msg, sizeof(struct bc_msg));
    packetbuf_set_datalen(sizeof(struct bc_msg));
    broadcast_send(&conn->bc);

    #if USR_DEBUG == 1
    float m = metric_q124_to_float(conn->metric);
    int ip = (int)m;
    int fp = (int)((m - ip) * 100);
    printf("rp-tree-build: sending beacon: seqn %d metric %d.%02d\n", conn->seqn, ip, fp);
    #endif
}

/*---------------------------------------------------------------------------*/

static void bc_recv(struct broadcast_conn* b_conn, const linkaddr_t *tx_addr) {
  uint16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI); //get rssi for metric computation
  if(rssi < RSSI_LOW_THR) return; // discard beacons with too low rssi

  if(packetbuf_datalen() != sizeof(struct bc_msg)) {
      #if USR_DEBUG == 1
      printf("rp_conn: broadcast message has wrong size\n");
      #endif
      return;
    }

  struct rp_conn* conn = (struct rp_conn*)(((uint8_t*)b_conn) - offsetof(struct rp_conn, bc));

  struct bc_msg msg; //get message from packet buffer
  memcpy(&msg, packetbuf_dataptr(), sizeof(struct bc_msg));
  float flt_adv = metric_q124_to_float(msg.metric_q124);
  
  /*get (or create) entry of the transmitter*/
  entry_t* tx_e = (entry_t*) nbr_table_get_from_lladdr(nbr_tbl, tx_addr);
  if(tx_e != NULL){ //if is an already known neighbor, then refresh the entry
    nbr_tbl_refresh(nbr_tbl, tx_addr);
    tx_e->adv_metric = msg.metric_q124;
  }
  else{ //otherwise, create new entry
    tx_e = (entry_t*) nbr_table_add_lladdr(nbr_tbl, tx_addr, NBR_TABLE_REASON_ROUTE, NULL);
    tx_e->type = NODE_NEIGHBOR;
    tx_e->age = clock_time();
    tx_e->nexthop = *tx_addr;
    tx_e->etx = etx_est_rssi(rssi);
    tx_e->num_tx = 0;
    tx_e->num_ack = 0;
    tx_e->adv_metric = msg.metric_q124;
   }

  /*For non sink nodes: if the beacon comes from a new epoch 
    reset your connection status and prepare to rebuild the tree from scratch */
    if(!conn->sink && msg.seqn > conn->seqn)
        reset_connection_status(conn, msg.seqn, conn->sink);

    /*process beacon*/
    //compute metric to the sink through the transmitter
    float new_mt = metric(flt_adv, tx_e->etx);

    /*if the metric is better(with some tolerance) than the current,
    then the node becomes the new parent, otherwise it stays neighbor*/
    float cur_mt_f = metric_q124_to_float(conn->metric);
    if(preferred(new_mt, cur_mt_f)){
        //update connection state
        linkaddr_copy(&conn->parent, tx_addr);
        conn->metric = metric_float_to_q124(new_mt);
        conn->hops = msg.hops + 1;

        //update entry
        tx_e->type = NODE_PARENT;
        // set the timers for the beacon forwarding and for the upsrteam report
        ctimer_set(&conn->beacon_timer, TREE_BEACON_FORWARD_DELAY, beacon_timer_cb, conn);
        ctimer_set(&subtree_report_timer, SUBTREE_REPORT_BASE_DEL(conn->hops), subtree_report_cb, conn);
        #if USR_DEBUG == 1
        float m = metric_q124_to_float(conn->metric);
        int ip = (int)m;
        int fp = (int)((m - ip) * 100);
        printf("rp-tree-build: updating parent from %02x:%02x to %02x:%02x, new metric %d.%02d, new hops %u (received beacon seqn %u)\n",
          conn->parent.u8[0], conn->parent.u8[1],
          tx_addr->u8[0], tx_addr->u8[1],
          ip, fp, msg.hops + 1, msg.seqn);
        #endif
    }
    else{
        /*Either the transmitter is a neighbor with a worse metric, or it is a child that is forwording its beacon.
        If it is a child, then it has to be added to the buffer if it is still advertising this node as
        a parent, otherwise it has to be removed from the buffer, because it found a better parent. */
        if(linkaddr_cmp(&msg.parent, &linkaddr_node_addr)){ //if the transmitter advertises this node as parent, then it is a child
            //update entry
            tx_e->type = NODE_CHILD;
            //update the buffer
            stat_addr_t tx_stat = {.addr = *tx_addr, .status = STATUS_ADD};
            conn->tpl_buf.stat_addr_arr[conn->tpl_buf.size++] = tx_stat;
            #if USR_DEBUG == 1
            float m = metric_q124_to_float(conn->metric);
            int ip = (int)m;
            int fp = (int)((m - ip) * 100);
            printf("rp-tree-build: new child %02x:%02x, my metric %d.%02d, my seqn %d\n",
                   tx_addr->u8[0], tx_addr->u8[1], ip, fp, conn->seqn);
            #endif
              
        }
        else{//either it is a neighbor or an old child
            if(tx_e->type == NODE_CHILD){
                //update entry
                tx_e->type = NODE_NEIGHBOR;
                //update the buffer (remove the entry)
                int i;
                for(i = 0; i < conn->tpl_buf.size; i++) {
                    if(linkaddr_cmp(&conn->tpl_buf.stat_addr_arr[i].addr, tx_addr)) {
                        //shift the array back
                        int j;
                        for(j = i; j < conn->tpl_buf.size - 1; j++) 
                            conn->tpl_buf.stat_addr_arr[j] = conn->tpl_buf.stat_addr_arr[j + 1];
                        conn->tpl_buf.size--;
                        break;
                    }
                }
              }
            //else it is a neighbor, no need to do anything (entry type is already up to date)
            #if USR_DEBUG == 1
            float m = metric_q124_to_float(conn->metric);
            int ip = (int)m;
            int fp = (int)((m - ip) * 100);
            printf("rp-tree-build: new neighbor %02x:%02x, my metric %d.%02d, my seqn %d\n",
                   tx_addr->u8[0], tx_addr->u8[1], ip, fp, conn->seqn);
            #endif
        }
      }
  }


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*----------------------------TOPOLOGY MAINTENANCE---------------------------*/


void subtree_report_cb(void* ptr){

    struct rp_conn* conn = (struct rp_conn*) ptr;
    if(conn->tpl_buf.size == 0) {
        ctimer_set(&subtree_report_timer, SUBTREE_REPORT_NODE_INTERVAL(conn->hops), subtree_report_cb, conn);
        return;
    }

    uint8_t rem = conn->tpl_buf.size - conn->buf_off;
    uint8_t frag_sz = (rem > RP_MAX_STAT_PER_FRAG) ? RP_MAX_STAT_PER_FRAG : rem;
    // build header
    packetbuf_clear();
    if(packetbuf_hdralloc(sizeof(struct uc_hdr))){
        struct uc_hdr hdr = {.type = UC_TYPE_REPORT, .d_addr = conn->parent, .s_addr = linkaddr_node_addr, .hops = 0};
        memcpy(packetbuf_hdrptr(), &hdr, sizeof(hdr));
    }
    else{
        #if USR_DEBUG == 1
        printf("ERROR: Failed to allocate unicast header!\n");
        #endif
        return;
    }

    //build payload
    uint8_t *pld = packetbuf_dataptr();
    *pld = frag_sz;
    memcpy(pld+1, &conn->tpl_buf.stat_addr_arr[conn->buf_off], frag_sz * sizeof(stat_addr_t));
    packetbuf_set_datalen(1 + frag_sz * sizeof(stat_addr_t));

    //send fragment
    conn->last_uc_daddr = conn->parent;
    unicast_send(&conn->uc, &conn->parent);
    
    conn->buf_off += frag_sz; //move offset

    //if all the buffer is sent, schedule next report, otherwise schedule next fragment
    if(conn->buf_off < conn->tpl_buf.size) 
        ctimer_set(&subtree_report_timer, CLOCK_SECOND / 50, subtree_report_cb, conn);
    else {
        //report completed, flush the buffer and schedule next
        flush_tpl_buf(conn);
        conn->buf_off = 0;
        ctimer_set(&subtree_report_timer, SUBTREE_REPORT_NODE_INTERVAL(conn->hops), subtree_report_cb, conn);
    }
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*----------------------------PARENT MANAGEMENT---------------------------*/

/*---------------------------------------------------------------------------*/
/*Helper  for change_parent: bufferize the subtree to send as topology report to the new parent*/
static void buff_subtree(nbr_table_t* nbr_tbl, struct rp_conn* conn){
    /*Flush topology buffer: no need to keep track of expired entries or topology changes,
    only the effective valod descendants need to the new parent, so we rebuild the buffer from scratch*/
    conn->tpl_buf.size = 0;
    entry_t* e;
    for(e=nbr_table_head(nbr_tbl); e != NULL; e = nbr_table_next(nbr_tbl, e)){
        //find all elements of the subtree
        if( !(e->type == NODE_DESCENDANT) && !(e->type == NODE_CHILD))
            continue;
        else{
            linkaddr_t d_addr = *nbr_table_get_lladdr(nbr_tbl, e);
            stat_addr_t d_stat = {.addr = d_addr, .status = STATUS_ADD};
            conn->tpl_buf.stat_addr_arr[conn->tpl_buf.size++] = d_stat;
        }
    }
  }

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void change_parent(void* ptr){ //change parent and mark as expired the old parent
    cb_args_t* args = (cb_args_t*) ptr;
    struct rp_conn* conn = args->conn;
    nbr_table_t* nbr_tbl =args->nbr_tbl;
   
    linkaddr_t old_par = conn->parent;

    //Iterate the neighbor table and find the best neighbor
    float bst_mt = FLT_MAX;
    entry_t* new_par_e = NULL;
    entry_t* e;
    for(e=nbr_table_head(nbr_tbl); e!=NULL; e=nbr_table_next(nbr_tbl, e)){
        if(e->type != NODE_NEIGHBOR) continue; //skip entries that are not neighbors

        float cnd_mt = metric(metric_q124_to_float(e->adv_metric), e->etx);
        if(cnd_mt < bst_mt){
            bst_mt = cnd_mt;
            new_par_e = e;
        }
    }

    entry_t* old_par_e = (entry_t*) nbr_table_get_from_lladdr(nbr_tbl, &old_par);
    if(old_par_e != NULL){
        old_par_e->type = NODE_NEIGHBOR; //downgrade the old parent to neighbor
        old_par_e->age = ALWAYS_INVALID_AGE; //set as expired
    }

    if(new_par_e != NULL){
        conn->parent = *(nbr_table_get_lladdr(nbr_tbl, new_par_e));
        conn->metric = metric_float_to_q124(bst_mt);
        new_par_e->type = NODE_PARENT;
        conn->hops = new_par_e->hops + 1;

        #if USR_DEBUG == 1
        metric_q124_t m = conn->metric;
        int ip = m >> 8;
        int fp = ((m & 0xFF) * 100 + 128) >> 8;
        printf("topology_report: parent change from %02x:%02x to %02x:%02x, my new metric %d.%02d, my seqn %d\n", 
           old_par.u8[0], old_par.u8[1], new_par_e->nexthop.u8[0], new_par_e->nexthop.u8[1], ip, fp, conn->seqn);
        #endif
        //Inform the new parent of the subtree
        buff_subtree(nbr_tbl, conn);
        subtree_report_cb(conn);
    }
    else{//if there are no neighbors available, disconnect from the network
        linkaddr_copy(&conn->parent, &linkaddr_null);
        #if USR_DEBUG == 1
        printf("topology_report: Node %02x:%02x did not find a parent, disconnecting from the network\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]); 
        #endif
        return;
  }
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*------------------------------unicast handling-----------------------------*/

static void uc_recv(struct unicast_conn* u_conn, const linkaddr_t* tx_addr){

    struct rp_conn* conn = (struct rp_conn*)( ((uint8_t*)u_conn) - offsetof(struct rp_conn, uc));


    // Check if the received unicast message looks legitimate
    if (packetbuf_datalen() < sizeof(struct uc_hdr)) {
      #if USR_DEBUG == 1
      printf("ERROR: Too short unicast packet %d. ", packetbuf_datalen());
      printf("Received packet of length %d from %02x:%02x\n", packetbuf_datalen(), tx_addr->u8[0], tx_addr->u8[1]);
      const uint8_t *raw_data = (uint8_t *)packetbuf_dataptr();
      int i;
      for(i = 0; i < packetbuf_datalen(); i++) 
        printf("%02x ", raw_data[i]);
      printf("\n");
      #endif
      return;
    }

    struct uc_hdr hdr;
    memcpy(&hdr, packetbuf_dataptr(), sizeof(hdr));
    packetbuf_hdrreduce(sizeof(hdr)); 
    hdr.hops = hdr.hops +1; //increment hop count in the header to be forwarded
    if(hdr.hops > MAX_PATH_LENGTH) return; //drop if reached the maximum path length

    #if USR_DEBUG == 1
    printf("[LOG] Node %02x:%02x RECEIVED packet from %02x:%02x originally sent by %02x:%02x (hops: %d)\n",
      linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
      tx_addr->u8[0], tx_addr->u8[1],
      hdr.s_addr.u8[0], hdr.s_addr.u8[1],
      hdr.hops); 
    #endif

    nbr_tbl_refresh(nbr_tbl, tx_addr); //refresh entry
    switch(hdr.type){
        case UC_TYPE_DATA: //application data pakcet
            //if this node is the destination, then call the application. Otherwise forward
            if(linkaddr_cmp(&hdr.d_addr, &linkaddr_node_addr))
              conn->callbacks->recv(&hdr.s_addr, hdr.hops); //call the recv callback function
            else
              forward_data(conn, hdr);
            break;

        case UC_TYPE_REPORT:
            #if USR_DEBUG == 1
            print_topology_report(tx_addr);
            printf("topology-report: report from child %02x:%02x\n", 
              tx_addr->u8[0], tx_addr->u8[1]);
            #endif

            /*Extract report forom the packet buffer*/
            if(packetbuf_datalen() < sizeof(uint8_t)){
              #if USR_DEBUG == 1
              printf("topology-report: ERROR, packet too short (%d bytes)\n", packetbuf_datalen());
              #endif
              return;
            }
            
            tpl_vec_t net_buf;
            uint8_t* dataptr = packetbuf_dataptr();
            net_buf.size = *dataptr; //set size of the incoming topology vector (first byte of the packet is size)
            dataptr++;
           // Ensure the packet contains the expected amount of data
             uint8_t exp_b = net_buf.size * sizeof(stat_addr_t);
             if((packetbuf_datalen() - 1) < exp_b) {
               #if USR_DEBUG == 1
               printf("ERROR: Insufficient data: expected %u bytes for %u entries\n", exp_b, net_buf.size);
               #endif
               return;
             }
             // Read each stat_addr_t from the packet into net_buf's array
             uint8_t i;
             for(i = 0; i < net_buf.size; i++)
                 memcpy(&net_buf.stat_addr_arr[i], dataptr + i * sizeof(stat_addr_t), sizeof(stat_addr_t));
            
          //update neighbor table with the incoming reports
            nbr_tbl_update(nbr_tbl, conn, tx_addr, net_buf);
            //if not sink, schedule the next report. Otherwise, flush the buffer
            if(!(conn->sink))
                ctimer_set(&subtree_report_timer, SUBTREE_REPORT_DELAY, subtree_report_cb, conn); //send the report in upstream, piggybacking the local information also
            else
                flush_tpl_buf(conn);
            break;  
          
        default:
            break;
    }
}

static void uc_sent(struct unicast_conn *c, int status, int num_tx){

  struct rp_conn* conn = (struct rp_conn*)(((uint8_t*)c) - offsetof(struct rp_conn, uc));
  entry_t* e = (entry_t*) nbr_table_get_from_lladdr(nbr_tbl, &conn->last_uc_daddr);

  if(e != NULL){
    e->num_tx += num_tx; //increment the number of transmissions with the MAC trasmissions
    if(status == MAC_TX_OK) e->num_ack++; //increment number of ACKs if the receiver acked
      e->etx = etx_update(e->num_tx, e->num_ack, e->etx, packetbuf_attr(PACKETBUF_ATTR_RSSI));
  }

  /*Update ETX*/

  if(e != NULL)
    e->num_tx += num_tx; //increment the number of transmissions with the MAC trasmissions

  switch(status){
    case MAC_TX_OK:
      #if USR_DEBUG == 1
      printf("rp: Packet sent successfully (ACK received), retransmissions: %d\n", num_tx);
      #endif
      nbr_tbl_refresh(nbr_tbl, &conn->last_uc_daddr); //refresh entry
      break;

    case MAC_TX_NOACK:
      #if USR_DEBUG == 1
      printf("rp: Packet transmission failed (NO ACK), retransmissions: %d.\n", num_tx);      
      #endif
      switch(e->type){
        case NODE_PARENT:
        //If the parent is not responding, then change parent
          #if USR_DEBUG == 1
          printf("rp: Changing parent because parent did not ACK\n");
          #endif
          e->age = ALWAYS_INVALID_AGE;
          nbr_tbl_cleanup_cb(&conn->clu_args);
          break;

        case NODE_CHILD:
        //remove the subtree
          #if USR_DEBUG == 1
          printf("rp: Removing child and subtree %02x:%02x from the routing table\n", conn->last_uc_daddr.u8[0], conn->last_uc_daddr.u8[1]);
          #endif
          e->age = ALWAYS_INVALID_AGE;
          nbr_tbl_cleanup_cb(&conn->clu_args);
          break;

        case NODE_NEIGHBOR:
          #if USR_DEBUG == 1
          printf("rp: Removing neighbor %02x:%02x from the routing table\n", conn->last_uc_daddr.u8[0], conn->last_uc_daddr.u8[1]);
          #endif
          e->age = ALWAYS_INVALID_AGE;
          nbr_tbl_cleanup_cb(&conn->clu_args);
          break;

        default:
          break;   
      }
      break;
      
    default:
      break;
  }
  
}


#if USR_DEBUG == 1

/*---------------------------------------------------------------------------*/
/* Prints the routing table for debug and monitoring */
/*---------------------------------------------------------------------------*/

static void rp_print_routing_table(struct rp_conn *conn){
  printf("--------------------------------------------------\n");
  printf("Routing Table for node %02x:%02x\n",
         linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
  int ip = (int)conn->metric;
  int fp = (int)((conn->metric - ip) * 100);
  printf("Parent: %02x:%02x   |   Metric: %d.%02d\n",
         conn->parent.u8[0], conn->parent.u8[1],
         ip, fp);
  printf("--------------------------------------------------\n");
  printf("   Dest    |  Next Hop |   Type   |  Metric |  Age (ticks)\n");
  printf("--------------------------------------------------\n");

  // NBR_TABLE iteration
  
  entry_t *e = nbr_table_head(nbr_tbl);
  while(e != NULL) {
    const linkaddr_t *dest = nbr_table_get_lladdr(nbr_tbl, e);

    char *type_str = "UNKNOWN";
    switch(e->type) {
      case 0: type_str = "PARENT";      break;
      case 1: type_str = "CHILD";       break;
      case 2: type_str = "DESCENDANT";  break;
      case 3: type_str = "NEIGHBOR";    break;
    }

    float m = metric(metric_q124_to_float(e->adv_metric), e->etx);
    int ip = (int)m;
    int fp = (int)((m - ip) * 100);
    printf(" %02x:%02x     | %02x:%02x     | %8s | %d.%02d | %10lu\n",
           dest->u8[0], dest->u8[1],
           e->nexthop.u8[0], e->nexthop.u8[1],
           type_str, 
           ip, fp,
           (clock_time() - e->age));

    e = nbr_table_next(nbr_tbl, e);
  }

  printf("--------------------------------------------------\n\n");
}



static void print_topology_report(const linkaddr_t* child_addr) {
  if(packetbuf_datalen() < sizeof(uint8_t)){
      printf("Topology Report: ERROR, packet too short (%d bytes)\n", packetbuf_datalen());
      return;
  }

  uint8_t* dataptr = packetbuf_dataptr();
  uint8_t report_size;
  memcpy(&report_size, dataptr, sizeof(uint8_t));
  dataptr += sizeof(uint8_t);

  printf("\n[TOPOLOGY REPORT] Received from Child %02x:%02x\n", child_addr->u8[0], child_addr->u8[1]);
  printf("------------------------------------------\n");
  printf(" Node Address  | Status \n");
  printf("------------------------------------------\n");

  uint8_t i;
  for(i = 0; i < report_size; i++) {
      stat_addr_t entry;
      memcpy(&entry, dataptr, sizeof(stat_addr_t));
      dataptr += sizeof(stat_addr_t);

      const char* status_str = (entry.status == STATUS_ADD) ? "Added" : "Removed";
      printf(" %02x:%02x        | %s\n", entry.addr.u8[0], entry.addr.u8[1], status_str);
  }
  printf("------------------------------------------\n\n");
}

#endif /* USR_DEBUG */