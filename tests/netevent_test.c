#include <stdio.h>
#include "netevent.h"

int main()
{
  
  int i = 0, n = 0, j=0; 
  uint16_t val = 0; 
  int started = 0; 
  
  const int MAXEVENT = 4; 
  struct event_t eh[MAXEVENT]; 

  NetEventHandle * pnh = NetEvent_new("10.0.0.2"); 
  NetEvent_setMask(pnh, 0, 0x10); 
  NetEvent_startEventRX(pnh); 
  
  for (i = 0; i < 1000; i++) {
    n = NetEvent_getEvents(pnh, eh, MAXEVENT); 
    
    //printf("received n = %d events\n", n); 
    for (j = 0; j < n; j++) {
      if (started == 0) {
	started = 1; 
      } else {
	if (eh[j].data[2] != val + 1) {
	  printf("Error\n"); 
	}
      }
      val = eh[j].data[2]; 
	
    }
  }
}
