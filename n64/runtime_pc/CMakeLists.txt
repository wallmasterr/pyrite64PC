# Minimal PC runtime (SDL3 + GLES2) for Pyrite64 projects
set(SDL_SHARED_DEFAULT OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${PYRITE64_ROOT}/vendored/SDL ${CMAKE_BINARY_DIR}/SDL EXCLUDE_FROM_ALL)

add_executable(p64_pc_runtime main.cpp)
set_target_properties(p64_pc_runtime PROPERTIES OUTPUT_NAME "${P64_ROM_NAME}_pc")
target_include_directories(p64_pc_runtime PRIVATE ${PYRITE64_ROOT}/vendored/SDL/include)
target_link_libraries(p64_pc_runtime PRIVATE SDL3::SDL3)
target_compile_definitions(p64_pc_runtime PRIVATE P64_PROJECT_PATH="${P64_PROJECT_PATH}")

# Output next to build directory for easy run
set_target_properties(p64_pc_runtime PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
