/*
 *
 * Student Name: Damiano Salvaterra
 *
 */

#include "nbr_tbl_utils.h"
#include "rp.h"
/*---------------------------------------------------------------------------*/

/*Checks in the routing table if there is a nexthop to dest. If not, it returns the parent*/
inline void nbr_tbl_lookup(nbr_table_t* nbr_tbl, linkaddr_t* nexthop, const linkaddr_t* dst_addr, const linkaddr_t* parent){
    const entry_t *entry = (entry_t *) nbr_table_get_from_lladdr(nbr_tbl, dst_addr);
  
    if (entry != NULL)
      linkaddr_copy(nexthop, &entry->nexthop);
    else
      linkaddr_copy(nexthop, parent); //default route to parent
}

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void remove_subtree(nbr_table_t* nbr_tbl, struct rp_conn* conn, linkaddr_t ch_addr){

  //remove the subtree: iterate the routing table to find entry of the subtree
  entry_t* tmp = nbr_table_head(nbr_tbl);
  while(tmp != NULL){
    entry_t* nxt_tmp = nbr_table_next(nbr_tbl, tmp); 

    if(linkaddr_cmp(&tmp->nexthop, &ch_addr)){ //entry being part of the child subtree (child is the root of the subtree)
      linkaddr_t des_addr = *nbr_table_get_lladdr(nbr_tbl, tmp); 
      nbr_table_remove(nbr_tbl, tmp); //remove from the routing table
      stat_addr_t des_stat = {.addr = des_addr, .status = STATUS_REMOVE}; //add to the topology buffer
      conn->tpl_buf.stat_addr_arr[conn->tpl_buf.size++] = des_stat;
      #if USR_DEBUG == 1
      printf("nbr_tbl: removing descedant %02x:%02x from subtree rooted in child entry %02x:%02x\n", des_stat.addr.u8[0], des_stat.addr.u8[1], ch_addr.u8[0], ch_addr.u8[1]);
      #endif
    }
    tmp = nxt_tmp;
  }
}


/*---------------------------------------------------------------------------*/


/*callback for flushing the routing table from expired entries*/
void nbr_tbl_cleanup_cb(void *ptr) {
   cb_args_t* args = (cb_args_t*) ptr;
   struct rp_conn* conn = args->conn;
   nbr_table_t* nbr_tbl =args->nbr_tbl;
   

   //need to do this in two sweeps to avoid dangling pointers while iterating the table
   entry_t* stales[NBR_TABLE_CONF_MAX_NEIGHBORS]; //array of stale entries
   uint8_t stales_count = 0;
   bool parent_change = false;

   // pass 1: iterate over the routing table and find expired entries (skip descendants since we don't know if they are expired) 
   entry_t* e;
   for (e = nbr_table_head(nbr_tbl); e != NULL; e= nbr_table_next(nbr_tbl, e))
    if(!(VALID(e->age)) && (e->type != NODE_DESCENDANT))
        stales[stales_count++] = e;
    

    //pass 2: removing the entries
  uint8_t i;
  for(i=0; i< stales_count; i++){
    
      if(stales[i]->type == NODE_CHILD)
          remove_subtree(nbr_tbl, conn, *nbr_table_get_lladdr(nbr_tbl, stales[i]));
      else if (stales[i]->type == NODE_PARENT){ //if the parent is being removed, then you need to change the parent
          nbr_table_remove(nbr_tbl, stales[i]);
          parent_change = true;
          conn->parent = linkaddr_null;
          //conn->tpl_buf.stat_addr_arr[conn->tpl_buf.size++] = (stat_addr_t){.addr = *nbr_table_get_lladdr(nbr_tbl, stales[i]), .status = STATUS_REMOVE};
      }
      else{ //if not a child nor a parent, then it is a neighbor (or a descendant during the state reset on receiving a new beacon)
          nbr_table_remove(nbr_tbl, stales[i]);
          //conn->tpl_buf.stat_addr_arr[conn->tpl_buf.size++] = (stat_addr_t){.addr = *nbr_table_get_lladdr(nbr_tbl, stales[i]), .status = STATUS_REMOVE};
      }
     
  
  }

    /* Schedule next cleanup */
    ctimer_reset(&conn->nbr_tbl_cleanup_timer);
    //change parent if the parent is expired
    if(parent_change) change_parent(args);
}

/*---------------------------------------------------------------------------*/

/*update the neighbor table based on the incoming topology report, and update the local topology buffer*/
void nbr_tbl_update(nbr_table_t* nbr_tbl, struct rp_conn* conn, const linkaddr_t* tx_addr, tpl_vec_t net_buf){

  entry_t* tx_entry = nbr_table_get_from_lladdr(nbr_tbl, tx_addr);
  if(tx_entry && tx_entry->type == NODE_NEIGHBOR){ //if it is a neighbor that chose this node as a parent, book the change into the buffer
    stat_addr_t tx_stat = {.addr = *tx_addr, .status = STATUS_ADD};
    conn->tpl_buf.stat_addr_arr[conn->tpl_buf.size++] = tx_stat;
    tx_entry->adv_metric = METRIC_Q124_INF; //set infinite metric to avoid loops
  } //else it is an already known child

  //update the routing table and the local buffer with the info contained in the topology report
  uint8_t i;
  for(i=0; i<net_buf.size; i++){ 
      if(conn->tpl_buf.size >= NBR_TABLE_CONF_MAX_NEIGHBORS) {
        #if USR_DEBUG
          uint8_t skip_count = net_buf.size - i;
          printf("nbr_tbl: buffer overflow, skipping %u entries starting from entry %02x:%02x\n",
                       skip_count, net_buf.stat_addr_arr[i].addr.u8[0], net_buf.stat_addr_arr[i].addr.u8[1]);
        #endif
          break;
      }
  
      //copy the report into the buffer
      //putting this line here implies that also entris in STATUS_ADD but already in this nbr_tbl
      //will be propagatd upwards: harmless, but is useless information.
      //This should be changed to avoid transmitting redundancies
      conn->tpl_buf.stat_addr_arr[conn->tpl_buf.size++] = net_buf.stat_addr_arr[i];
  
      const linkaddr_t* d_addr = &(net_buf.stat_addr_arr[i].addr);
      uint8_t status = net_buf.stat_addr_arr[i].status;
  
      if(status == STATUS_ADD){ //add descendant entry in the neighbor table
        //add descendant entry in the neighbor table
          entry_t* d_entry = (entry_t*) nbr_table_add_lladdr(nbr_tbl, d_addr, NBR_TABLE_REASON_ROUTE, NULL);
          if(d_entry != NULL){
            d_entry->type = NODE_DESCENDANT;
            d_entry->adv_metric = METRIC_Q124_INF; //avoid loops
            d_entry->age = ALWAYS_VALID_AGE; //no need to keep track of it: the topology report will remove the descendants if necessary
            d_entry->hops = 0xFF; // not used
            d_entry->nexthop = *tx_addr; 
          }
          #if USR_DEBUG == 1
          printf("nbr_tbl: new descendant %02x:%02x, from child %02x:%02x\n", 
            d_addr->u8[0], d_addr->u8[1], tx_addr->u8[0], tx_addr->u8[1]);
          #endif
      }
  
      else if (status == STATUS_REMOVE){
          entry_t* d_entry = nbr_table_get_from_lladdr(nbr_tbl, d_addr);
          if(d_entry != NULL){//if the entry exist, remove it
            nbr_table_remove(nbr_tbl, d_entry);
            #if USR_DEBUG == 1
            printf("nbr_tbl: removing descendant %02x:%02x, from subtree rooted in child %02x:%02x\n", 
              d_addr->u8[0], d_addr->u8[1], tx_addr->u8[0], tx_addr->u8[1]);
            #endif
          }
      }
    }

}
