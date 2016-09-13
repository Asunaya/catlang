cmake_minimum_required(VERSION 2.8)

include(CheckCXXCompilerFlag)
set (GCC_FLAGS "-Wall -Wextra -pedantic -Wno-strict-aliasing -std=c++14 -Ofast")
CHECK_CXX_COMPILER_FLAG(${GCC_FLAGS} HAS_GCC_FLAGS)
if (HAS_GCC_FLAGS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_FLAGS}")
endif()
# C4503 = decorated name length exceeded, name was truncated
set (MSVC_FLAGS "/wd4503")
CHECK_CXX_COMPILER_FLAG(${MSVC_FLAGS} HAS_MSVC_FLAGS)
if (HAS_MSVC_FLAGS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MSVC_FLAGS}")
endif()

file(GLOB catlang_src
    "catlang/*.h"
    "catlang/*.cpp"
)

add_executable(catlang ${catlang_src})
