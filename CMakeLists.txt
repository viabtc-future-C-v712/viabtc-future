cmake_minimum_required(VERSION 3.11)

execute_process(COMMAND git describe --tags
    TIMEOUT 5
    OUTPUT_VARIABLE GIT_TAG_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(COMMAND git log -1 --pretty=format:%h
    TIMEOUT 5
    OUTPUT_VARIABLE BUILD_LOG_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

project(viabtc-future)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

# set(CMAKE_C_COMPILER gcc)
# set(CMAKE_CXX_COMPILER g++)

include_directories(${PROJECT_SOURCE_DIR}/network/inc)
file(GLOB sources "./network/src/*.c")
add_library(network STATIC ${sources})

include_directories(${PROJECT_SOURCE_DIR}/utils/inc)
file(GLOB sources "./utils/src/*.c")
add_library(utils STATIC ${sources} network)
target_link_libraries(utils network)

add_subdirectory(accesshttp)
add_subdirectory(matchengine)
add_subdirectory(marketprice)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${GIT_TAG_VERSION}-${BUILD_LOG_VERSION}")

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME rt)

include(CPack)