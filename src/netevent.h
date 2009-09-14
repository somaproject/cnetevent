#ifndef NETEVENT_H
#define NETEVENT_H
//#include "structmember.h"
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>

static const int DATARETXPORT = 4400; 

static const int EVENTRXRETXPORT = 5500; // event retransmission requests

static const int EVENTTXPORT = 5100; // events from software to the soma bus

// FROM SOMA TO SOFTWARE
// These are ports that we, the software, listen on. Soma sets these as the 
// DESTINATION port in packets that it sends. 

static const int EVENTRXPORT = 5000; 

#define EVENTLEN 6

#define NETEVENT_SUCCESS 0
#define NETEVENT_EGETEVENTS -1
#define NETEVENT_ESENDERROR -2
#define NETEVENT_ESENDERROR_TIMEOUT -3

struct  event_t
{
  unsigned char cmd; 
  unsigned char src; 
  uint16_t data[EVENTLEN-1]; 
} ; 

struct eventListItem_t; 

struct  eventListItem_t
{
  struct event_t e; 
  struct eventListItem_t * elt; 
}; 


struct EventList_t
{
  struct eventListItem_t * eltHead; 
  struct eventListItem_t * eltTail; 
  pthread_mutex_t mutex;
  pthread_mutex_t size_mutex; 
  pthread_cond_t size_thold_cv; 
  int size; 
} ; 

struct NetworkSharedThreadState_t
{
  struct EventList_t * pel; 
  pthread_mutex_t running_mutex;
  int running; 
  char * rxValidLUT; 
}  ; 

typedef struct {
  //  PyObject *rxSet; /* RX set list */ 
  //PyObject *ip; /* Soma device IP address */ 
  int number;
  struct NetworkSharedThreadState_t * pnss; 
  pthread_t * pNetworkThread; 
  int txsocket; 
  char * ip; 
  char * rxValidLUT; 
} NetEventHandle;

int setupRXSocket(void); 
uint32_t getEvents(int sock, char * rxValidLUT, 
		   struct EventList_t * eventlist); 

NetEventHandle * NetEvent_new(char * addrstr); 
void NetEvent_free(NetEventHandle *); 
void NetEvent_pthread_runner(struct NetworkSharedThreadState_t * pnss); 
void NetEvent_startEventRX(NetEventHandle * nh); 

void NetEvent_stopEventRX(NetEventHandle * nh); 
int NetEvent_getEvents(NetEventHandle * nh, struct  event_t * etgt, int MAXEVENTS); 

int NetEvent_sendEvent(NetEventHandle * nh, struct event_t * e, uint8_t *  addrs); 

int NetEvent_setMask(NetEventHandle * nh, int src, int cmd); 
int NetEvent_unsetMask(NetEventHandle * nh, int src, int cmd); 

#endif //NETEVENT_H
