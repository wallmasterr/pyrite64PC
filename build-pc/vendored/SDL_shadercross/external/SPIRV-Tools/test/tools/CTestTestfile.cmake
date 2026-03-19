# CMake generated Testfile for 
# Source directory: C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/test/tools
# Build directory: C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/build-pc/vendored/SDL_shadercross/external/SPIRV-Tools/test/tools
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[spirv-tools_expect_unittests]=] "C:/Python314/python.exe" "-m" "unittest" "expect_unittest.py")
set_tests_properties([=[spirv-tools_expect_unittests]=] PROPERTIES  WORKING_DIRECTORY "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/test/tools" _BACKTRACE_TRIPLES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/test/tools/CMakeLists.txt;15;add_test;C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/test/tools/CMakeLists.txt;0;")
add_test([=[spirv-tools_spirv_test_framework_unittests]=] "C:/Python314/python.exe" "-m" "unittest" "spirv_test_framework_unittest.py")
set_tests_properties([=[spirv-tools_spirv_test_framework_unittests]=] PROPERTIES  WORKING_DIRECTORY "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/test/tools" _BACKTRACE_TRIPLES "C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/test/tools/CMakeLists.txt;18;add_test;C:/Users/PC/Documents/GitHub/pyrite64PC/pyrite64PC/vendored/SDL_shadercross/external/SPIRV-Tools/test/tools/CMakeLists.txt;0;")
subdirs("opt")
subdirs("objdump")
