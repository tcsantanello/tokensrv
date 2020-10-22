CMAKE_MINIMUM_REQUIRED( VERSION 3.0 )

PROJECT( tokenrest )

SET( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake )

SET( CMAKE_CXX_STANDARD 11 )
SET( CMAKE_CXX_STANDARD_REQUIRED ON )
SET( CMAKE_CXX_EXTENSIONS OFF )
SET( CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0 -Wall -ftemplate-backtrace-limit=0" )

IF ( EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake" )
  INCLUDE( "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake" )
  CONAN_BASIC_SETUP( )
ENDIF( )

# ##########################################
# OpenSSL
#
SET( OPENSSL_ROOT_DIR ${CONAN_BUILD_DIRS_OPENSSL} )
FIND_PACKAGE( OpenSSL )

# ##########################################
# Boost
#
SET( BOOST_ROOT ${CONAN_BUILD_DIRS_BOOST} )
SET( Boost_NO_BOOST_CMAKE     ON )
SET( Boost_USE_STATIC_LIBS    ON )
SET( Boost_USE_MULTITHREADED  ON )
FIND_PACKAGE( Boost COMPONENTS REQUIRED
  filesystem
  regex
  system
  thread
  program_options
)

#CONFIGURE_FILE(
#  ${CMAKE_SOURCE_DIR}/include/config.h.in
#  ${CMAKE_BINARY_DIR}/include/config.h
#)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${Boost_INCLUDE_DIRS}
  ${CONAN_INCLUDE_DIRS}
)

LINK_DIRECTORIES( ${Boost_LIBRARY_DIRS} ${CONAN_LIB_DIRS} )
ADD_DEFINITIONS( ${CONAN_DEFINES} )

# ##########################################
# SpeedLog
#
FIND_PATH( SPDLOG_INCLUDE_PATH
  NAMES spdlog/spdlog.h
  PATHS
  ${CONAN_INCLUDE_DIRS}
  /usr/include
  /usr/local/include
  )

IF ( NOT SPDLOG_INCLUDE_PATH )
  MESSAGE( FATAL_ERROR "Missing Speedlog" )
ELSE( )
  MESSAGE( STATUS "Speedlog found: ${SPDLOG_INCLUDE_PATH}" )
ENDIF( )

# ##########################################
# URI Header
#
FIND_PATH( URI_INCLUDE_PATH
  NAMES uri/uri.hh
  PATHS
  ${CONAN_INCLUDE_DIRS}
  /usr/include
  /usr/local/include
)

IF ( NOT URI_INCLUDE_PATH )
  MESSAGE( FATAL_ERROR "Missing URI Header" )
ELSE( )
  MESSAGE( STATUS "URI Header found: ${URI_INCLUDE_PATH}" )
ENDIF( )

##############################################################################
##############################################################################
##############################################################################

ENABLE_TESTING( )

ADD_SUBDIRECTORY( src )
#ADD_SUBDIRECTORY( test )
