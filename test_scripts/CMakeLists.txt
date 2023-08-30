find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_scripts tests.cpp)
target_link_libraries(test_scripts ${GTEST_LIBRARIES} pthread storage linked_list)