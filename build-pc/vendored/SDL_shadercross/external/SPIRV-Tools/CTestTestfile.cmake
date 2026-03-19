# CMake generated Testfile for 
# Source directory: C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools
# Build directory: C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[spirv-tools-copyrights]=] "C:/Python314/python.exe" "utils/check_copyright.py")
set_tests_properties([=[spirv-tools-copyrights]=] PROPERTIES  WORKING_DIRECTORY "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools" _BACKTRACE_TRIPLES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/CMakeLists.txt;355;add_test;C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/CMakeLists.txt;0;")
subdirs("external")
subdirs("source")
subdirs("tools")
subdirs("test")
subdirs("examples")
