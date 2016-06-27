cmake_minimum_required(VERSION 2.8)

project(sqs-batch-with-message-attributes-test)

### gtest stuff

include(Ctest)

if (CMAKE_VERSION VERSION_LESS 3.2)
  set(UPDATE_DISCONNECTED_IF_AVAILABLE "")
else()
  set(UPDATE_DISCONNECTED_IF_AVAILABLE "UPDATE_DISCONNECTED 1")
endif()

include(DownloadProject.cmake)

download_project(PROJ                googletest
                 GIT_REPOSITORY      https://github.com/google/googletest.git
                 GIT_TAG             master
                 ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)

add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

if (CMAKE_VERSION VERSION_LESS 2.8.11)
    include_directories("${gtest_SOURCE_DIR}/include"
                        "${gmock_SOURCE_DIR}/include")
endif()

### build project

file(GLOB SQS_BATCH_WITH_MESSAGE_ATTRIBUTES_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB SQS_BATCH_WITH_MESSAGE_ATTRIBUTES_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
file(GLOB SQS_BATCH_WITH_MESSAGE_ATTRIBUTES_UNIFIED_SRC
 ${SQS_BATCH_WITH_MESSAGE_ATTRIBUTES_SRC}
 ${SQS_BATCH_WITH_MESSAGE_ATTRIBUTES_HEADERS}
)

find_package(aws-sdk-cpp)

enable_testing()

add_executable(runSQSBatchWithMessageAttributesTest ${SQS_BATCH_WITH_MESSAGE_ATTRIBUTES_UNIFIED_SRC})

target_link_libraries(runSQSBatchWithMessageAttributesTest gtest gtest_main aws-cpp-sdk-sqs)
