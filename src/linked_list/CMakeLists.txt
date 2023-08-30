add_library(linked_list SHARED linked_list.c)
if(CMAKE_BUILD_TYPE EQUAL "DEBUG")
    target_compile_definitions(storage PUBLIC DEBUG)
endif()