add_library(storage SHARED storage.c)
if(CMAKE_BUILD_TYPE EQUAL "DEBUG")
    target_compile_definitions(storage PUBLIC DEBUG)
endif()