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
include CMakeFiles/helper.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/helper.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/helper.dir/flags.make

CMakeFiles/helper.dir/helper.cpp.o: CMakeFiles/helper.dir/flags.make
CMakeFiles/helper.dir/helper.cpp.o: ../helper.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/curc/mp-http/cpp_proxy/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/helper.dir/helper.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/helper.dir/helper.cpp.o -c /home/curc/mp-http/cpp_proxy/helper.cpp

CMakeFiles/helper.dir/helper.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/helper.dir/helper.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/curc/mp-http/cpp_proxy/helper.cpp > CMakeFiles/helper.dir/helper.cpp.i

CMakeFiles/helper.dir/helper.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/helper.dir/helper.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/curc/mp-http/cpp_proxy/helper.cpp -o CMakeFiles/helper.dir/helper.cpp.s

CMakeFiles/helper.dir/scheduler.cpp.o: CMakeFiles/helper.dir/flags.make
CMakeFiles/helper.dir/scheduler.cpp.o: ../scheduler.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/curc/mp-http/cpp_proxy/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/helper.dir/scheduler.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/helper.dir/scheduler.cpp.o -c /home/curc/mp-http/cpp_proxy/scheduler.cpp

CMakeFiles/helper.dir/scheduler.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/helper.dir/scheduler.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/curc/mp-http/cpp_proxy/scheduler.cpp > CMakeFiles/helper.dir/scheduler.cpp.i

CMakeFiles/helper.dir/scheduler.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/helper.dir/scheduler.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/curc/mp-http/cpp_proxy/scheduler.cpp -o CMakeFiles/helper.dir/scheduler.cpp.s

# Object files for target helper
helper_OBJECTS = \
"CMakeFiles/helper.dir/helper.cpp.o" \
"CMakeFiles/helper.dir/scheduler.cpp.o"

# External object files for target helper
helper_EXTERNAL_OBJECTS =

libhelper.a: CMakeFiles/helper.dir/helper.cpp.o
libhelper.a: CMakeFiles/helper.dir/scheduler.cpp.o
libhelper.a: CMakeFiles/helper.dir/build.make
libhelper.a: CMakeFiles/helper.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/curc/mp-http/cpp_proxy/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX static library libhelper.a"
	$(CMAKE_COMMAND) -P CMakeFiles/helper.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/helper.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/helper.dir/build: libhelper.a

.PHONY : CMakeFiles/helper.dir/build

CMakeFiles/helper.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/helper.dir/cmake_clean.cmake
.PHONY : CMakeFiles/helper.dir/clean

CMakeFiles/helper.dir/depend:
	cd /home/curc/mp-http/cpp_proxy/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/curc/mp-http/cpp_proxy /home/curc/mp-http/cpp_proxy /home/curc/mp-http/cpp_proxy/build /home/curc/mp-http/cpp_proxy/build /home/curc/mp-http/cpp_proxy/build/CMakeFiles/helper.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/helper.dir/depend

