/**
 * \file
 *         Implementation of the parent's bracelet
 * \author
 *         Marco Bendinelli
 */
#include "contiki.h"
#include "net/rime/rime.h"
#include "project-conf.h"
#include "random.h"
#include <stdio.h>
#include <string.h>

/* Various states */
static uint8_t state;
#define STATE_PAIRING           0
#define STATE_OPERATION         1
/*---------------------------------------------------------------------------*/
static struct etimer et_pairing;
static struct ctimer ct_missing;
#define PAIRING_TIMER           5
#define MISSING_TIMER           60
/*---------------------------------------------------------------------------*/
static uint8_t connect_attempt; // Number of broadcast messages sent
static linkaddr_t addr; // Address of the child
static struct unicast_conn uc;
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
#define STOP_PAIRING         "FOUND" // Special message to stop the pairing phase
#define FALLING             "FALLING" // INFO message
#define FALL                  "FALL" // Specific FALL message
#define MISSING             "MISSING" // Specific MISSING message
static char message[40] = {STOP_PAIRING}; // Message to send
static char child_coordinates[40]; // Coordinates of the child updated
/*---------------------------------------------------------------------------*/
PROCESS(parent_bracelet_process, "Parent's bracelet");
AUTOSTART_PROCESSES(&parent_bracelet_process);

/**
 * It sends to the child a unicast special message "FOUND",
 * to stop the pairing phase.
 * It changes the state of the FSM in to STATE_OPERATION.
 */
static void start_operation_mode() {
  
  printf("Child's bracelet found\n");
  
  packetbuf_copyfrom(message, strlen(message));

  if(!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
    unicast_send(&uc, &addr);
  }
  
  broadcast_close(&broadcast);
  state = STATE_OPERATION;
  printf("Operation mode starts\n");
}

/**
 * It checks if the received key is equal to the stored one.
 * If yes, it saves the address of the sender and
 * it starts the operation mode.
 * 
 * @param *c : the broadcast connection 
 * @param *from : the address of the sender
 */
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  char received_key[KEY_DIMENSION];
  char sender_address[10];
  
  sprintf(sender_address, "%d.%d", from->u8[0], from->u8[1]);
  sprintf(received_key, "%s", (char *)packetbuf_dataptr());
    
  printf("Broadcast message received from %s: %s\n",
         sender_address, received_key);
  
  if (!strcmp(received_key,PRODUCT_KEY)){
    addr.u8[0] = from->u8[0];
    addr.u8[1] = from->u8[1];   
    start_operation_mode();
  }
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/**
 * It checks the address of the sender
 * 
 * @param *from : the address of the sender 
 * @return 1 if the address is equal to the one of the paired child,
 *         0 otherwise
 */
static int check_address(const linkaddr_t *from) {

  if (from->u8[0] == addr.u8[0] && from->u8[1] == addr.u8[1])
    return 1;
  else
    return 0;
}

/**
 * It prints the MISSING message and it restarts the timer from the previous expiration time.
 * 
 * @param *ptr : data pointer 
 */
static void callback(void *ptr)
{
  printf("%s,%s\n", MISSING, child_coordinates);
  ctimer_reset(&ct_missing);
}

/**
 * It starts a ctimer of 60 seconds, when the ctimer expires,
 * it calls the callback function with the data pointer as argument. 
 */
void init(void)
{
  ctimer_set(&ct_missing, CLOCK_SECOND * MISSING_TIMER, callback, NULL);
}

/**
 * If: it has received a FOUND message and the FSM is in the STATE_PAIRING,
 * it moves the FSM in to the STATE_OPERATION.
 * Else if: it has received a message from the pairing child and
 * the FSM is in the STATE_OPERATION, it resets the MISSING timer and
 * it prints a FALL message if the child is falling.
 * 
 * @param *c : the unicast connection 
 * @param *from : the address of the sender
 */
static void recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
  char *status;
  char *ptr;
  
  if (check_address(from) == 1 && state == STATE_OPERATION) {
    
    init();
    printf("%s\n", (char *)packetbuf_dataptr());
       
    ptr = strtok((char *)packetbuf_dataptr(), ",");
    ptr = strtok(NULL, ",");
    sprintf(child_coordinates, "%s", ptr);    
    strcat(child_coordinates, ",");
    ptr = strtok(NULL, ",");
    strcat(child_coordinates, ptr);
    
    status = strtok((char *)packetbuf_dataptr(), ":");
    status = strtok(NULL, ":");
    
    if (!strcmp(status, FALLING)){
      printf("%s,%s\n", FALL, child_coordinates);
    }
  }
  
  else if (state == STATE_PAIRING) {
  
    printf("Unicast message received from %d.%d: %s\n",
           from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

    if (!strcmp((char *)packetbuf_dataptr(), STOP_PAIRING)){
      broadcast_close(&broadcast);
      printf("Found by the child's bracelet\n");
      addr.u8[0] = from->u8[0];
      addr.u8[1] = from->u8[1];
      state = STATE_OPERATION;
      printf("Operation mode starts\n");
    }
  }
}

/**
 * It just sends a unicast message.
 * 
 * @param *c : the unicast connection 
 * @param status 
 * @param num_tx 
 */
static void sent_uc(struct unicast_conn *c, int status, int num_tx)
{
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

  if(linkaddr_cmp(dest, &linkaddr_null)) {
    return;
  }

  printf("Message sent to %d.%d\n",
    dest->u8[0], dest->u8[1]);
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc, sent_uc};

/**
 * It sets the initial configuration of the program:
 * it sets the FSM in to the STATE_PAIRING and
 * it starts the pairing timer.
 */
static void init_config()
{
  connect_attempt = 1;
  printf("Pairing mode starts\n");
  state = STATE_PAIRING;
  etimer_set(&et_pairing, CLOCK_SECOND * PAIRING_TIMER);
}

/**
 * The main FSM of the program:
 * in the STATE_PAIRING it sends periodically a broadcast message.
 * I entered for completeness also the STATE_OPERATION,
 * even if during that it does nothing.
 */
static void state_machine(void)
{
  switch(state) {
    case STATE_PAIRING:
      
      packetbuf_copyfrom(PRODUCT_KEY, KEY_DIMENSION);
      broadcast_send(&broadcast);
      printf("Searching for parent's bracelet... (broadcast messages sent: %d, key: %s)\n", connect_attempt, PRODUCT_KEY);
      connect_attempt++;
      etimer_set(&et_pairing, CLOCK_SECOND * PAIRING_TIMER);      
      break;
      
    case STATE_OPERATION:

      break;		
  }

}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(parent_bracelet_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_EXITHANDLER(unicast_close(&uc);)
  
  PROCESS_BEGIN();
  
  init_config();
  broadcast_open(&broadcast, 129, &broadcast_call);
  unicast_open(&uc, 146, &unicast_callbacks);

  while(1) {
    
    PROCESS_YIELD();

    if (ev == PROCESS_EVENT_TIMER) {
      state_machine();
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
