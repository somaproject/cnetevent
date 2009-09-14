
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(somanetevent)

find_package(Threads)

# default build uses -O2 -g 
IF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE RelWithDebInfo)
ENDIF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

SET(LIBRARY_VERSION "1.0")
SET(LIBRARY_SOVERSION "0")



# boost requirements
find_package(PkgConfig)

ENABLE_TESTING()
INCLUDE(CTest)

include_directories(src/)

add_subdirectory(src)
add_subdirectory(tests)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/somanetevent-1.0.pc.in 
  ${CMAKE_CURRENT_BINARY_DIR}/somanetevent-1.0.pc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/somanetevent-1.0.pc 
  DESTINATION lib/pkgconfig)
