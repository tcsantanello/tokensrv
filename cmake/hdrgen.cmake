FUNCTION(HDRGEN)
  CMAKE_PARSE_ARGUMENTS(HDRGEN "" "FILENAME;HEADER;VARIABLE" "" ${ARGN})

  IF(NOT HDRGEN_VARIABLE)
    GET_FILENAME_COMPONENT(HDRGEN_VARIABLE "${HDRGEN_FILENAME}" NAME)
    STRING(MAKE_C_IDENTIFIER "${HDRGEN_VARIABLE}" HDRGEN_VARIABLE)
  ENDIF()

  IF(NOT HDRGEN_HEADER)
    GET_FILENAME_COMPONENT(HDRGEN_HEADER "${HDRGEN_FILENAME}" NAME_WE)
    SET(HDRGEN_HEADER "${HDRGEN_HEADER}.h")
  ENDIF()

  FILE(READ ${HDRGEN_FILENAME} content HEX)
  FILE(SIZE ${HDRGEN_FILENAME} size)

  STRING(TOUPPER "${content}00" content)
  STRING(REGEX REPLACE "(................................)" "\\1\n  " content
                       "${content}"
  )
  STRING(REGEX REPLACE "([0-9A-F][0-9A-F])" "0x\\1, " content "${content}")

  GET_FILENAME_COMPONENT(sentinal "${HDRGEN_HEADER}" NAME)
  STRING(TOUPPER "__${sentinal}__" sentinal)
  STRING(REGEX REPLACE "[^a-zA-Z0-9]" "_" sentinal ${sentinal})

  FILE(
    WRITE ${HDRGEN_HEADER}
    "#ifndef ${sentinal}
#define ${sentinal}

#include <stdint.h>

const size_t  ${HDRGEN_VARIABLE}_sz = ${size};
const uint8_t ${HDRGEN_VARIABLE}[]  = {
  ${content}
};

#endif
"
  )
ENDFUNCTION()
