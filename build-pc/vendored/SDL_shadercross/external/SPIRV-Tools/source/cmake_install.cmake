# Install script for directory: C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/source

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/pyrite64")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/msys64/ucrt64/bin/objdump.exe")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/opt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/reduce/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/fuzz/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/link/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/lint/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/diff/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/libSPIRV-Tools.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/libSPIRV-Tools-shared.dll.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/libSPIRV-Tools-shared.dll")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/libSPIRV-Tools-shared.dll" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/libSPIRV-Tools-shared.dll")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "C:/msys64/ucrt64/bin/strip.exe" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/libSPIRV-Tools-shared.dll")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SPIRV-Tools/SPIRV-ToolsTarget.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SPIRV-Tools/SPIRV-ToolsTarget.cmake"
         "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/CMakeFiles/Export/8d64bc3ee653eb494dac6da36f93ee74/SPIRV-ToolsTarget.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SPIRV-Tools/SPIRV-ToolsTarget-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SPIRV-Tools/SPIRV-ToolsTarget.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SPIRV-Tools" TYPE FILE FILES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/CMakeFiles/Export/8d64bc3ee653eb494dac6da36f93ee74/SPIRV-ToolsTarget.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SPIRV-Tools" TYPE FILE FILES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/CMakeFiles/Export/8d64bc3ee653eb494dac6da36f93ee74/SPIRV-ToolsTarget-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SPIRV-Tools" TYPE FILE FILES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/SPIRV-ToolsConfig.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/source/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
