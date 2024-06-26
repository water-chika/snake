cmake_minimum_required(VERSION 3.20)

project(vulkan_start)

find_package(Vulkan REQUIRED)
add_subdirectory(vulkan_helper)

add_executable(snake
    main.cpp
    cube.vert
    cube_vert.spv
    cube.frag
    cube_frag.spv
)
target_link_libraries(snake PUBLIC vulkan_helper)
set_target_properties(snake PROPERTIES CXX_STANDARD 23)

add_custom_command(OUTPUT vert.spv
  COMMAND Vulkan::glslangValidator --target-env vulkan1.3
              ${CMAKE_CURRENT_SOURCE_DIR}/test.vert
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/test.vert
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test.vert Vulkan::glslangValidator)

add_custom_command(OUTPUT cube_vert.spv
  COMMAND Vulkan::glslangValidator --target-env vulkan1.3
              ${CMAKE_CURRENT_SOURCE_DIR}/cube.vert -o cube_vert.spv
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/cube.vert
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cube.vert Vulkan::glslangValidator)
