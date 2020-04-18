cmake_minimum_required(VERSION 3.17)

enable_language(CXX)

project(linkollector-cli LANGUAGES CXX VERSION 0.0.1)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
endif()

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /nologo")
endif()

# clang and gcc don't default-print colored diagnostics when invoked from Ninja
if("${CMAKE_GENERATOR}" STREQUAL "Ninja" AND
("${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang" OR
("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND
        NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9))))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color")
    if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fansi-escape-codes")
    endif()
endif()

if(MSVC)
    foreach (flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REPLACE "/W3" "" ${flag_var} "${${flag_var}}")
        string(REPLACE "/EHc" "" ${flag_var} "${${flag_var}}")
        string(REPLACE "/EHsc" "" ${flag_var} "${${flag_var}}")
        string(REPLACE "/GR" "" ${flag_var} "${${flag_var}}")
    endforeach()

    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
        set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-imsvc ")
    else()
        set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "/external:I ")
    endif()
endif()

add_executable(${PROJECT_NAME} src/main.cpp)

# Disable exceptions
if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _HAS_EXCEPTIONS=0)
    target_compile_options(${PROJECT_NAME} PRIVATE /EHa- /EHs-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-exceptions)
endif()

# Disable RTTI
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /GR-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti)
endif()

# Sanitize MSVC
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)

    target_compile_definitions(${PROJECT_NAME} PRIVATE NOGDI)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VC_EXTRALEAN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)

    if(NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
        target_compile_options(${PROJECT_NAME} PRIVATE /experimental:external /external:W0 /permissive-)
    endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # gcc 5
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wcast-qual
        -Wconversion
        -Wctor-dtor-privacy
        -Wdeprecated-declarations
        -Wdisabled-optimization
        -Wdouble-promotion
        -Wextra
        -Wformat=2
        -Wlogical-op
        -Wmissing-include-dirs
        -Wnoexcept
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wpedantic
        -Wpointer-arith
        -Wredundant-decls
        -Wshadow
        -Wsign-conversion
        -Wsized-deallocation
        -Wtrampolines
        -Wundef
        -Wunused
        -Wunused-parameter
        -Wuseless-cast
        -Wvector-operation-performance
        -Wwrite-strings
        -pedantic-errors
    )

    # gcc 6
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.1)
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wduplicated-cond
            -Wmisleading-indentation
            -Wnull-dereference
            -Wshift-overflow=2
        )
    endif()

    # gcc 7
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.1)
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wduplicated-branches
        )
    endif()

    # gcc 9
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 9.1)
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wzero-as-null-pointer-constant
        )
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Weverything
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-padded
        -Wno-return-std-move-in-c++11
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /we4289
        /w14296
        /w14311
        /w14545
        /w14546
        /w14547
        /w14549
        /w14555
        /w14619
        /w14640
        /w14826
        /w14905
        /w14906
        /w14928
    )
endif()

add_subdirectory(extern)

target_link_libraries(${PROJECT_NAME} PUBLIC libzmq-static)
get_target_property(ZMQ_INCLUDES_TO_SYSTEM libzmq-static INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${ZMQ_INCLUDES_TO_SYSTEM})


