# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/curc/mp-http/cpp_proxy

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/curc/mp-http/cpp_proxy/build

# Include any dependencies generated for this target.
include CMakeFiles/proxy.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/proxy.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/proxy.dir/flags.make

CMakeFiles/proxy.dir/proxy.cpp.o: CMakeFiles/proxy.dir/flags.make
CMakeFiles/proxy.dir/proxy.cpp.o: ../proxy.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/curc/mp-http/cpp_proxy/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/proxy.dir/proxy.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/proxy.dir/proxy.cpp.o -c /home/curc/mp-http/cpp_proxy/proxy.cpp

CMakeFiles/proxy.dir/proxy.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/proxy.dir/proxy.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/curc/mp-http/cpp_proxy/proxy.cpp > CMakeFiles/proxy.dir/proxy.cpp.i

CMakeFiles/proxy.dir/proxy.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/proxy.dir/proxy.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/curc/mp-http/cpp_proxy/proxy.cpp -o CMakeFiles/proxy.dir/proxy.cpp.s

# Object files for target proxy
proxy_OBJECTS = \
"CMakeFiles/proxy.dir/proxy.cpp.o"

# External object files for target proxy
proxy_EXTERNAL_OBJECTS =

proxy: CMakeFiles/proxy.dir/proxy.cpp.o
proxy: CMakeFiles/proxy.dir/build.make
proxy: libhelper.a
proxy: CMakeFiles/proxy.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/curc/mp-http/cpp_proxy/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable proxy"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/proxy.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/proxy.dir/build: proxy

.PHONY : CMakeFiles/proxy.dir/build

CMakeFiles/proxy.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/proxy.dir/cmake_clean.cmake
.PHONY : CMakeFiles/proxy.dir/clean

CMakeFiles/proxy.dir/depend:
	cd /home/curc/mp-http/cpp_proxy/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/curc/mp-http/cpp_proxy /home/curc/mp-http/cpp_proxy /home/curc/mp-http/cpp_proxy/build /home/curc/mp-http/cpp_proxy/build /home/curc/mp-http/cpp_proxy/build/CMakeFiles/proxy.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/proxy.dir/depend

