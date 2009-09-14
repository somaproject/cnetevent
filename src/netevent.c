#include "netevent.h"
#include <errno.h>

void walkEventList(struct eventListItem_t * li)
{
  struct eventListItem_t * current = li; 
  //printf("walking list beginning at %d\n", li); 
  while (current != NULL) {
    printf("e.cmd = %d, e.src = %d\n", 
	   current->e.cmd, 
	   current->e.src); 

    current = current->elt; 
  }

}


NetEventHandle * 
NetEvent_new(char * addrstr)
{
  
  NetEventHandle * nh = malloc(sizeof(NetEventHandle)); 
  
  nh->ip = malloc(strlen(addrstr) + 1); 
  strcpy(nh->ip, addrstr); 

  nh->txsocket = 0; 
  nh->txsocket = socket(AF_INET, SOCK_DGRAM, 17); 
  nh->rxValidLUT = malloc(256*256); 
  bzero(nh->rxValidLUT, 256*256); 

  // now the shared network state
  nh->pnss = malloc(sizeof(struct NetworkSharedThreadState_t)); 

  pthread_mutex_init(&(nh->pnss->running_mutex), NULL); 	
  nh->pnss->running = 0; 
	
  // and the shared network state element list
  nh->pnss->pel = malloc(sizeof(struct EventList_t)); 
  nh->pnss->pel->size = 0; 

  pthread_mutex_init(&(nh->pnss->pel->mutex), NULL); 
  pthread_mutex_init(&(nh->pnss->pel->size_mutex), NULL); 

  
  pthread_cond_init(&(nh->pnss->pel->size_thold_cv), NULL); 
	
       
  nh->pnss->pel->eltHead = NULL; 

  nh->pNetworkThread = malloc(sizeof(pthread_t)); 
  nh->pnss->rxValidLUT = (char * ) malloc(256*256); 
  bzero(nh->pnss->rxValidLUT, 256*256); 

  nh->number = 0;
  return nh; 
}

void NetEvent_free(NetEventHandle * nh )
{
  // FIXME: I'm pretty sure we leak here! 
  NetEvent_stopEventRX(nh); 
  free(nh); 

}

void pthread_runner(struct NetworkSharedThreadState_t * pnss)
{
  
  struct EventList_t * pel = pnss->pel; 
  
  int socket = setupRXSocket(); 

  pthread_mutex_lock(&(pnss->running_mutex)); 

  pnss->running  = 1; 
  
  pthread_mutex_unlock(&(pnss->running_mutex)); 
    
  
  char firstLoop = 1; 
  uint32_t rxseq, prevseq =0; 
  char wasrunning; 

  while( 1) { 
    pthread_mutex_lock(&(pnss->running_mutex)); 

    wasrunning = pnss->running ; 
    
    pthread_mutex_unlock(&(pnss->running_mutex)); 

    if (wasrunning == 0 ) 
      break; 
    
    // now use select as a time-out-able interface
    fd_set readfds; 
    FD_SET(socket, &readfds); 
    struct timeval timeout; 
    timeout.tv_sec = 1; 
    timeout.tv_usec = 000; 
    int retval = select(socket+1, &readfds, NULL, NULL,  &timeout); 
    if (retval == -1) { 
      printf("Error with select\n"); 
    } else if (retval == 0) {
      //printf("timeout\n"); 
    } else { // at least one FD
      if(FD_ISSET(socket, &readfds) && retval > 0) {
	struct EventList_t newEventList;
	newEventList.eltHead = NULL;
	
	rxseq  = getEvents(socket, pnss->rxValidLUT, &newEventList);
	if (firstLoop) {
	  firstLoop = 0;
	  prevseq = rxseq -1;
	} 
	//printf("rxseq = %8.8x\n", rxseq); 
	// check for sequential RX
	if (prevseq + 1 == rxseq ) {
	  // good
	  //printf("rx about to acquire size mutex, newEventList.size=%d rxseq = %d\n", newEventList.size, rxseq); 
	  //printf("about to acquire\n"); 
	  pthread_mutex_lock(&(pel->size_mutex)); 	
	  pthread_mutex_lock(&(pel->mutex));
	  //printf("success\n"); 

	  // if pel is empty, then we append this to it
	  if (pel->eltHead == NULL) {
	    pel->eltHead = newEventList.eltHead; 
	    pel->eltTail = newEventList.eltTail; 
	  } else { 
	    pel->eltTail->elt = newEventList.eltHead; 
	    pel->eltTail = newEventList.eltTail; 
	  }
	  
	  
	  // check if we added anything; if we did, update size
	  if (newEventList.size > 0) 
	    {	  
	      pel->size += newEventList.size; 
	      //printf("added %d to pel, pel.size = %d\n", newEventList.size, pel->size); 
	    }
	  
	  pthread_mutex_unlock(&(pel->mutex));
	  
	  if (pel->size > 0 ){
	    
	    pthread_cond_signal(&(pel->size_thold_cv)); 
	  }		
	  
	  pthread_mutex_unlock(&(pel->size_mutex)); 	  
	  
	} else {
	  printf("Oops, we dropped one, prevseq = %d, rxseq = %d \n", prevseq, rxseq);
	}
	prevseq = rxseq; 
      } else {
	printf("ISSET is false\n"); 
      }

    }
  }
  

}

void 
NetEvent_startEventRX(NetEventHandle * nh)
{
  int i = 0; 
  // build the constant-time LUT
  // copy from network handle
   for (i = 0; i < 256 * 256; i++) {
    //nh->pnss->rxValidLUT[cmd + src*256] = 1; 
    nh->pnss->rxValidLUT[i] = nh->rxValidLUT[i]; 
  }
  
  // now we fork a thread and begin queueing up events

  pthread_create((void*)&(nh->pNetworkThread), NULL, 
		 (void *)pthread_runner, nh->pnss); 

  char oldrunning = 0; 
  while (!oldrunning) {
    pthread_mutex_lock(&(nh->pnss->running_mutex)); 
    
    oldrunning = nh->pnss->running; 
    pthread_mutex_unlock(&(nh->pnss->running_mutex)); 
  }
  
}

void 
NetEvent_stopEventRX(NetEventHandle * nh)
{

  pthread_mutex_lock(&(nh->pnss->running_mutex)); 
  nh->pnss->running = 0; 
  pthread_mutex_unlock(&(nh->pnss->running_mutex)); 
  pthread_join(*(nh->pNetworkThread), 0); 

  // flush the buffer

	  
  pthread_mutex_lock(&(nh->pnss->pel->mutex));

  struct EventList_t * pel = nh->pnss->pel; 
  struct eventListItem_t * p = pel->eltHead; 
  while (p != NULL) {
    struct eventListItem_t * nextp = p->elt; 
    free(p); 
    p = nextp; 
  }
  pel->eltHead = NULL; 
  pel->eltTail = NULL; 
  pthread_mutex_unlock(&(nh->pnss->pel->mutex)); 

  pthread_mutex_lock(&(pel->size_mutex)); 
  pel->size =0; 
  pthread_mutex_unlock(&(pel->size_mutex)); 

}

int 
NetEvent_getEvents(NetEventHandle * nh, struct event_t * etgt, int MAXEVENTS)
{
  /*
    return up to MAXEVENTS in the event_t array passed as etgt. 

    return value is number of events, or -1 if error
  */
  
  struct EventList_t * pel = nh->pnss->pel; 

  // check the size
  //printf("pre-size is %d\n",  pel->size); 
	   
   
  pthread_mutex_lock(&(pel->size_mutex)) ;
/*   printf("PyNetEvent_getEvents(PyNetEvent* nh): size is %d\n",   */
/*  	  pel->size);   */
  if (pel->size == 0 ) {
    struct timespec timeWait; 
    struct timeval timeNow; 
    gettimeofday(&timeNow, NULL);
    timeWait.tv_sec = timeNow.tv_sec + 1; 
    timeWait.tv_nsec = timeNow.tv_usec; 
    
    // use cond_timedwait to periodically pass control back
    // so we can receive control-C and other signals
    int res = pthread_cond_timedwait(&(pel->size_thold_cv), 
				     &(pel->size_mutex), &timeWait); 
    if (res == ETIMEDOUT) {
      pthread_mutex_unlock(&(pel->size_mutex)); 
      //printf("timeout?\n"); 
      return NETEVENT_EGETEVENTS; 

    }
  }

  //printf("post-size is %d\n",  pel->size); 
	   

  pthread_mutex_lock(&(pel->mutex)); 

  struct eventListItem_t * phead = pel->eltHead; 


  int pos = 0; 

  while(phead != NULL && pos < MAXEVENTS) {
    struct eventListItem_t * curhead = phead; 
    
    //PyObject * outtuple = eventToPyTuple(&(curhead->e)); 
    //PyList_Append(outlist, outtuple); 
    memcpy(etgt +  pos, &(curhead->e), sizeof(struct event_t));     

    pos += 1; 
    phead = curhead->elt; 
    pel->eltHead = phead; 
    pel->size -= 1; 
    free(curhead); 
  }
  if (pel->eltHead == NULL) {
    pel->eltTail = NULL; 
  }

  pthread_mutex_unlock(&(pel->mutex)); 
  pthread_mutex_unlock(&(pel->size_mutex)); 


  return pos; 

  
}

int 
NetEvent_sendEvent(NetEventHandle * nh, struct event_t * e, uint8_t *  addrs)
{
  /* 
     e is a pointer to an event to send
     addrs is a pointer to a uint8[10] array of event addresses
     
     return ESENDERROR on failure, 0 otherwise
  */

  struct sockaddr_in saServer; 
  
  int sock = nh->txsocket; 
  memset(&saServer, sizeof(saServer), 0); 
  saServer.sin_family = AF_INET; 
  saServer.sin_port = htons(EVENTTXPORT);  
  
  inet_aton(nh->ip, &saServer.sin_addr); 
  // construct nonce
  
  char buffer[1500]; 
  bzero(buffer, 1500); 
  
  uint16_t hnonce, nnonce; 
  hnonce = rand(); 

  nnonce = htons(hnonce); 
  size_t bpos = 0; 
  //printf("nonce = %d\n", hnonce); 
  memcpy(&buffer[bpos], &nnonce, sizeof(nnonce)); 
  bpos += 2; 

  uint16_t hecnt, necnt; 
  hecnt = 1 ; 
  necnt = htons(hecnt); 
  memcpy(&buffer[bpos], &necnt, sizeof(necnt)); 
  bpos += sizeof(necnt); 

  int i; 

  // copy the addresses
  for (i = 0; i < 10; i++)
    {
      memcpy(&buffer[bpos], &addrs[i], sizeof(uint8_t)); 
      bpos += sizeof(uint8_t); 
    }
  
  // then the event data
  memcpy(&buffer[bpos], &e->cmd, 1); 
  bpos += 1; 
  memcpy(&buffer[bpos], &e->src, 1); 
  bpos += 1; 

  for (i = 0; i < 5; i++)
    {
      uint16_t nedata, hedata; 
      hedata = e->data[i]; 
      nedata = htons(hedata); 
      memcpy(&buffer[bpos], &nedata, sizeof(uint16_t)); 
      bpos += sizeof(uint16_t); 
    }
  bpos += 12;
  
  // single event
  uint16_t success = 0; 
  int noncesuccess = 0;
  uint16_t nrxnonce, hrxnonce; 

  while (! success) {
    //printf("sendto\n"); 
    sendto(sock, buffer, bpos, 0, 
	   (struct sockaddr*)&saServer, sizeof(saServer)); 
    
    bzero(buffer, 1500); 

    fd_set readfds; 
    FD_SET(sock, &readfds); 
    struct timeval timeout; 
    timeout.tv_sec = 1; 
    timeout.tv_usec = 0; 
    int retval = select(sock+1, &readfds, NULL, NULL,  &timeout); 

    if (retval == -1) { 
      // Error in select waiting for EventTX response from soma
      return NETEVENT_ESENDERROR; 
    } else if (retval == 0) {
      // Timed out waiting for EventTX response from soma
      return NETEVENT_ESENDERROR_TIMEOUT; 
    }

    int rxlen = recv(sock, buffer, 1500, 0); 

    if (rxlen < 4) {
      printf("RX len was too small\n"); 
    }
    
    memcpy(&nrxnonce, buffer, sizeof(nrxnonce)); 
    hrxnonce = ntohs(nrxnonce); 

    // extract out success/failure data
    success = buffer[3]; 
    if (hrxnonce == hnonce) {
      noncesuccess = 1; 
    } else {
      noncesuccess = 0; 
    }
    

    if (!success || !noncesuccess) {
      printf("TX Response failed! success = %d, sentnonce = %4.4X, rxnonce = %4.4X\n",
	     success, hnonce, hrxnonce); 
      return NETEVENT_ESENDERROR; 
    }
  }

  return NETEVENT_SUCCESS;  
}


int setupRXSocket()
{

  int sock; 

  struct sockaddr_in si_me;
  
    
  sock = socket(AF_INET, SOCK_DGRAM, 17); 
  
  memset((char *) &si_me, sizeof(si_me), 0);

  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(EVENTRXPORT); 

  si_me.sin_addr.s_addr = INADDR_ANY; 
  
  int optval = 1; 

  // confiugre socket for reuse
  optval = 1; 
  int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
	     &optval, sizeof (optval)); 

  optval = 4000000; 
  res = setsockopt (sock, SOL_SOCKET, SO_RCVBUF, 
		    (const void *) &optval, sizeof(optval)); 

  socklen_t optlen;   
  res = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, 
		   (void *) &optval, &optlen); 

  res =  bind(sock, (struct sockaddr*)&si_me, sizeof(si_me)); 

    
  return sock; 
}

uint32_t getEvents(int sock, char * rxValidLUT, 
		   struct EventList_t * eventlist)
{
  /*
    returns the sequence number for us to "deal with"
    and creates a 

  */ 
  const int EBUFSIZE = 1550; 
  char buffer[EBUFSIZE]; 

  struct sockaddr_in sfrom; 
  socklen_t fromlen = sizeof(sfrom); 
  
  size_t len = recvfrom(sock, buffer, EBUFSIZE, 
			0, (struct sockaddr*)&sfrom, &fromlen); 
  
  // extract out events
  //printf("getEvents len = %d\n", len); 
  uint32_t seq = ntohl(*((int *) &buffer[0])); 

  
  // decode the transmited event set into an array of events 
  
  size_t bpos = 4; 
  
  struct eventListItem_t * curelt = NULL;
  int addedcnt = 0; 

  int totalevents = 0;

  while ((bpos+1) < len) 
    { 
      uint16_t neventlen, eventlen;
      memcpy(&neventlen, &buffer[bpos], sizeof(neventlen));
      eventlen = ntohs(neventlen);
      bpos += 2;
      int evtnum;
      totalevents += eventlen; 
      for (evtnum = 0; evtnum < eventlen; evtnum++)
	{
	  // extract out individual events
	  struct event_t evt;
	  evt.cmd = buffer[bpos];
	  bpos++;
	  
	  evt.src = buffer[bpos];
	  bpos++;
	  
	  // we need to be much more careful about extracting out the 
	  // event data here
	  int i =0; 
	  for (i = 0; i < EVENTLEN-1; i++) {
	    uint16_t nedata, hedata; 
	    memcpy(&nedata, &buffer[bpos], sizeof(uint16_t)); 
	    hedata = ntohs(nedata); 
	    evt.data[i] = hedata; 
	    bpos += sizeof(uint16_t); 
	  }
	  
	  // now, is this one of ours?
 	  if (rxValidLUT[evt.cmd + evt.src*256] != 0) { 

 	    // now we add the thing 
	    struct eventListItem_t * newelt; 
	    newelt = malloc(sizeof(struct eventListItem_t)); 
	    bzero(newelt, sizeof(struct eventListItem_t)); 
	    newelt->elt = NULL; 
	    if (curelt == NULL ) {
	      eventlist->eltHead = newelt; 
	      eventlist->eltTail = newelt; 
	      curelt = newelt; 
	    } else {
	      curelt->elt = newelt; 
	      curelt = newelt; 
	      
	      eventlist->eltTail = curelt; 
	      
	    }
	    addedcnt += 1; 
	    curelt->e = evt;
	  }
	
	}
    }
  eventlist->size = addedcnt; 
  //printf("getEvents: Total events: %d\n", totalevents); 
  return seq;
  
}


int NetEvent_setMask(NetEventHandle * nh, int src, int cmd)
{
  int i; 
    //nh->pnss->rxValidLUT[cmd + src*256] = 1; 
  int offset = cmd + src * 256; 

  nh->rxValidLUT[cmd + src * 256] = 1; 
}

int NetEvent_unsetMask(NetEventHandle * nh, int src, int cmd)
{
  nh->rxValidLUT[cmd + src * 256] = 0; 


}
