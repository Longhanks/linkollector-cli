include(FetchContent)

FetchContent_Declare(
        zmq
        GIT_REPOSITORY    git@github.com:zeromq/libzmq.git
        GIT_TAG           v4.3.2
)

FetchContent_GetProperties(zmq)

if(NOT zmq_POPULATED)
    FetchContent_Populate(zmq)
    set(BUILD_SHARED OFF CACHE INTERNAL "")
    set(BUILD_STATIC ON CACHE INTERNAL "")
    set(WITH_PERF_TOOL OFF CACHE INTERNAL "")
    set(ZMQ_BUILD_TESTS OFF CACHE INTERNAL "")
    set(ENABLE_CPACK OFF CACHE INTERNAL "")

    if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
        set(POLLER "kqueue" CACHE INTERNAL "")
    endif()

    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
        if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
        endif()
    endif()

    add_subdirectory(${zmq_SOURCE_DIR} ${zmq_BINARY_DIR})
endif()
