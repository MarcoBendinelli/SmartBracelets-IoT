/**
 * \file
 *         Implementation of the child's bracelet
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
#define STATE_PAIRING          0
#define STATE_OPERATION        1 
/*---------------------------------------------------------------------------*/
static struct etimer et_operation;
static struct etimer et_pairing;
#define PAIRING_TIMER          5
#define OPERATION_TIMER        10
/*---------------------------------------------------------------------------*/
static uint8_t connect_attempt; // Number of broadcast messages sent
static linkaddr_t addr; // Address of the parent
static struct unicast_conn uc;
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
#define STOP_PAIRING         "FOUND" // Special message to stop the pairing phase
/* Various INFO messages*/
#define STANDING            "STANDING"
#define WALKING             "WALKING"
#define RUNNING             "RUNNING"
#define FALLING             "FALLING"
static char message[40] = {STOP_PAIRING};
/* Description of the child's status */
struct child_status
{
  char status[10];
};
static struct child_status child_status[4]; // Array of the possible child's status
/*---------------------------------------------------------------------------*/
static int counter_status[4]; // Counters of the number of the pulled specific status
static int status_counter = 0; // Counter of the total number of pulled status
/*---------------------------------------------------------------------------*/
PROCESS(child_bracelet_process, "Child's bracelet");
AUTOSTART_PROCESSES(&child_bracelet_process);

/**
 * It sends to the parent a unicast special message "FOUND",
 * to stop the pairing phase.
 * It changes the state of the FSM in to STATE_OPERATION.
 */
static void start_operation_mode() {
  
  printf("Parent's bracelet found\n");
  
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
  
  if (!strcmp(received_key,PRODUCT_KEY))
    addr.u8[0] = from->u8[0];
    addr.u8[1] = from->u8[1];   
    start_operation_mode();
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
 * If: it has received a FOUND message and the FSM is in the STATE_PAIRING,
 * it moves the FSM in to the STATE_OPERATION.
 * Else if: it has received a message from the pairing child and
 * the FSM is in the STATE_OPERATION, it shows the received message
 * (implemented for completeness).
 * 
 * @param *c : the unicast connection 
 * @param *from : the address of the sender
 */
static void recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
  if (check_address(from) == 1 && state == STATE_OPERATION) {
    printf("%s\n", (char *)packetbuf_dataptr());
  }
  
  else if (state == STATE_PAIRING) {
    printf("Unicast message received from %d.%d: %s\n",
           from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
         
    if (!strcmp((char *)packetbuf_dataptr(), STOP_PAIRING)){
      broadcast_close(&broadcast);
      printf("Found by the parent's bracelet\n");
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
 * It resets the counters of the pulled status.
 */
static void reset_probabilities(){
  int i;
  
  status_counter = 0;
  
  for (i = 0; i < 4; i++)
    counter_status[i] = 1;
}

/**
 * It sets the initial configuration of the program:
 * it sets the FSM in to the STATE_PAIRING,
 * it starts the pairing timer and it resets the counters.
 */
static void init_config()
{  
  sprintf(child_status[0].status, "%s", STANDING);
  sprintf(child_status[1].status, "%s", WALKING);
  sprintf(child_status[2].status, "%s", RUNNING);
  sprintf(child_status[3].status, "%s", FALLING);
    
  connect_attempt = 1;
  printf("Pairing mode starts\n");
  state = STATE_PAIRING;
  etimer_set(&et_pairing, CLOCK_SECOND * PAIRING_TIMER);
  reset_probabilities();
}

/**
 * It pulls a child's status with the following probabilities:
 * P(STANDING) = P(WALKING) = P(RUNNING) = 0.3 and P(FALLING) = 0.1.
 * 
 * @return the pulled status
 */
static char* read_status()
{
  int index;
  int probabilistically_valid = 0;
  
  status_counter++;
  
  while(probabilistically_valid == 0) {

    index = random_rand() % 4;

    if (counter_status[index] % 4 != 0 && index != 3) {
       probabilistically_valid = 1;
       counter_status[index]++;
       
    } else if (counter_status[3] % 2 != 0) {
       probabilistically_valid = 1;
       counter_status[index]++;      
    }
    
    if (status_counter % 10 == 0) {
      reset_probabilities();
    }
  }
  
  return child_status[index].status;
}

/**
 * The main FSM of the program:
 * in the STATE_PAIRING it sends periodically a broadcast message;
 * in the STATE_OPERATION it sends periodically a unicast message
 * with the status of the child.
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

      sprintf(message, "INFO:%s,X:%d,Y:%d", read_status(), random_rand(), random_rand());
      packetbuf_copyfrom(message, strlen(message));

      if(!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
        unicast_send(&uc, &addr);
      }

      etimer_set(&et_operation, CLOCK_SECOND * OPERATION_TIMER);

      break;		
  }

}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(child_bracelet_process, ev, data)
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
