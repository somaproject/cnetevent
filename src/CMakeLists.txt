set (somanetevent_SRCS
  netevent.c)

set (somanetevent_HEADERS
  netevent.h)

ADD_LIBRARY(somanetevent SHARED
  ${somanetevent_SRCS}
)

SET_TARGET_PROPERTIES(somanetevent
  PROPERTIES VERSION ${LIBRARY_VERSION}
  SOVERSION ${LIBRARY_SOVERSION})

install( TARGETS somanetevent LIBRARY
  DESTINATION lib)

install(FILES ${somanetevent_HEADERS}
  DESTINATION
  include/${PROJECT_NAME}-${LIBRARY_VERSION}/${PROJECT_NAME})
