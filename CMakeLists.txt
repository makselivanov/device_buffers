cmake_minimum_required(VERSION 3.12)
project(my_fuse)

add_subdirectory(src/linked_list)
add_subdirectory(src/storage)

add_executable(${PROJECT_NAME} fuse-tmpfs.c)
target_link_libraries(${PROJECT_NAME} linked_list storage fuse)
target_compile_options(${PROJECT_NAME} PUBLIC -D_FILE_OFFSET_BITS=64)

add_subdirectory(test_scripts)
