# Look for aasdk in the sibling directory
set(AASDK_SEARCH_DIR "${CMAKE_CURRENT_LIST_DIR}/../../aasdk")
message(STATUS "DEBUG: CMAKE_CURRENT_LIST_DIR=${CMAKE_CURRENT_LIST_DIR}")
message(STATUS "DEBUG: AASDK_SEARCH_DIR=${AASDK_SEARCH_DIR}")

find_path(AASDK_INCLUDE_DIR
    aasdk/Version.hpp
    PATHS ${AASDK_SEARCH_DIR}/include
    NO_DEFAULT_PATH
)

find_path(AASDK_PROTO_INCLUDE_DIR
    aasdk_proto/AbsoluteInputEventData.pb.h
    PATHS ${AASDK_SEARCH_DIR}/build
    NO_DEFAULT_PATH
)

find_library(AASDK_LIBRARY
    NAMES aasdk
    PATHS ${AASDK_SEARCH_DIR}/lib
    NO_DEFAULT_PATH
)

find_library(AASDK_PROTO_LIBRARY
    NAMES aasdk_proto
    PATHS ${AASDK_SEARCH_DIR}/lib
    NO_DEFAULT_PATH
)

if (AASDK_INCLUDE_DIR AND AASDK_PROTO_INCLUDE_DIR AND AASDK_LIBRARY AND AASDK_PROTO_LIBRARY)
    set(AASDK_FOUND TRUE)
endif()
  
if (AASDK_FOUND)
    if (NOT aasdk_FIND_QUIETLY)
        message(STATUS "Found aasdk:")
        message(STATUS " - Includes: ${AASDK_INCLUDE_DIR}")
        message(STATUS " - Includes: ${AASDK_PROTO_INCLUDE_DIR}")
        message(STATUS " - Library: ${AASDK_LIBRARY}")
        message(STATUS " - Proto Library: ${AASDK_PROTO_LIBRARY}")
    endif()
    add_library(aasdk INTERFACE)
    target_include_directories(aasdk INTERFACE ${AASDK_INCLUDE_DIR} ${AASDK_PROTO_INCLUDE_DIR})
    target_link_libraries(aasdk INTERFACE ${AASDK_LIBRARY} ${AASDK_PROTO_LIBRARY})
else()
    if (aasdk_FIND_REQUIRED)
        if(AASDK_INCLUDE_DIR AND NOT AASDK_PROTO_INCLUDE_DIR)
            message(FATAL_ERROR "aasdk was found but not built.  Perform an in-source build.")
        else()
            message(FATAL_ERROR "Could not find aasdk")
        endif()
    endif()
endif()

mark_as_advanced(AASDK_INCLUDE_DIRS AASDK_LIBRARIES)
