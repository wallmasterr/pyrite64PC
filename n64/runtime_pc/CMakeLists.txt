# Minimal PC runtime (SDL3 + GLES2) for Pyrite64 projects
set(SDL_SHARED_DEFAULT OFF)
set(BUILD_SHARED_LIBS OFF)
# Only build the native Windows video driver so we don't fall back to dummy/offscreen
# (those can fail without setting SDL error, giving "no SDL error" + GetLastError 0).
if(WIN32)
  set(SDL_DUMMYVIDEO OFF CACHE BOOL "" FORCE)
  set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(${PYRITE64_ROOT}/vendored/SDL ${CMAKE_BINARY_DIR}/SDL EXCLUDE_FROM_ALL)

set(PCRT_PLATFORM_SOURCES main.cpp pc_platform.c)
add_executable(p64_pc_runtime ${PCRT_PLATFORM_SOURCES})
set_target_properties(p64_pc_runtime PROPERTIES OUTPUT_NAME "${P64_ROM_NAME}_pc")
target_include_directories(p64_pc_runtime PRIVATE ${PYRITE64_ROOT}/vendored/SDL/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(p64_pc_runtime PRIVATE SDL3::SDL3)
if(WIN32)
  # Match SDL's Windows link libs so video/events/ole etc. resolve when linking static SDL
  target_link_libraries(p64_pc_runtime PRIVATE
    opengl32
    kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32 hid)
  # Use console subsystem so running from PowerShell/terminal works; -mwindows can break video init
  # set_target_properties(p64_pc_runtime PROPERTIES LINK_FLAGS "-mwindows")
endif()
target_compile_definitions(p64_pc_runtime PRIVATE P64_PROJECT_PATH="${P64_PROJECT_PATH}")

# Output next to build directory for easy run
set_target_properties(p64_pc_runtime PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Copy MinGW/UCRT64 runtime DLLs into build folder so exe runs without MSYS2 in PATH
if(WIN32)
  get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(PCRT_DLLS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
  foreach(DLL ${PCRT_DLLS})
    if(EXISTS "${MINGW_BIN_DIR}/${DLL}")
      add_custom_command(TARGET p64_pc_runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MINGW_BIN_DIR}/${DLL}" "$<TARGET_FILE_DIR:p64_pc_runtime>"
        COMMENT "Copying ${DLL} to build folder")
    endif()
  endforeach()
endif()
