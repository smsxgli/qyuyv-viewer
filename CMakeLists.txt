cmake_minimum_required(VERSION 3.16...3.23)

project(yuv_viewer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Gui)


add_executable(${PROJECT_NAME})

# add_compile_options(
#     $<$<CONFIG:Release>:-fsanitize=address,undefined>
# )
# add_link_options(
#     $<$<CONFIG:Release>:-fsanitize=address,undefined>
# )

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_sources(
    ${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/yuv_src.c
        ${CMAKE_CURRENT_LIST_DIR}/src/main.cc 
        ${CMAKE_CURRENT_LIST_DIR}/src/yuv_viewer.cc 
        ${CMAKE_CURRENT_LIST_DIR}/src/yuv_worker.cc
        ${CMAKE_CURRENT_LIST_DIR}/include/yuv_viewer.hh
        ${CMAKE_CURRENT_LIST_DIR}/include/yuv_worker.hh
)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Gui)
set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION TRUE
)
