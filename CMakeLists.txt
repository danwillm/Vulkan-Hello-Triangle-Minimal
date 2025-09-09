cmake_minimum_required(VERSION 3.28)
project(Hello-Vulkan)
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_subdirectory(lib/glm)
add_subdirectory(lib/glfw)

add_executable(program main.cpp)
target_link_libraries(program PRIVATE glfw glm ${Vulkan_LIBRARIES})
target_include_directories(program PRIVATE ${Vulkan_INCLUDE_DIRS})

# ---------- Shaders ----------
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

# Find glslc (use CMake's FindVulkan hint if available)
if (DEFINED Vulkan_GLSLC_EXECUTABLE)
    set(GLSLC ${Vulkan_GLSLC_EXECUTABLE})
else()
    find_program(GLSLC glslc
            HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
            REQUIRED)
endif()

# Correct GLOB syntax: variable FIRST, then options
file(GLOB SHADER_FILES CONFIGURE_DEPENDS
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
        "${SHADER_SOURCE_DIR}/*.comp"
        "${SHADER_SOURCE_DIR}/*.geom"
        "${SHADER_SOURCE_DIR}/*.tesc"
        "${SHADER_SOURCE_DIR}/*.tese"
        "${SHADER_SOURCE_DIR}/*.mesh"
        "${SHADER_SOURCE_DIR}/*.task"
        "${SHADER_SOURCE_DIR}/*.rgen"
        "${SHADER_SOURCE_DIR}/*.rchit"
        "${SHADER_SOURCE_DIR}/*.rmiss"
)

set(SPV_SHADERS)
foreach(SRC IN LISTS SHADER_FILES)
    get_filename_component(BASE ${SRC} NAME)   # e.g. foo.vert
    set(SPV "${SHADER_BINARY_DIR}/${BASE}.spv") # e.g. foo.vert.spv

    add_custom_command(
            OUTPUT ${SPV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
            COMMAND ${GLSLC} -o ${SPV} ${SRC}
            MAIN_DEPENDENCY ${SRC}
            VERBATIM
            COMMENT "Compiling ${BASE}"
    )

    list(APPEND SPV_SHADERS ${SPV})
endforeach()

# Use a name that does NOT collide with the 'shaders' directory
add_custom_target(compile_shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(program compile_shaders)
