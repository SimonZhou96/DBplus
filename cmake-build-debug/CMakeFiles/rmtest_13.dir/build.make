# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

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
CMAKE_SOURCE_DIR = /mnt/d/software/repositories

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/d/software/repositories/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/rmtest_13.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/rmtest_13.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/rmtest_13.dir/flags.make

CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o: CMakeFiles/rmtest_13.dir/flags.make
CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o: ../rm/rmtest_13.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/d/software/repositories/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o -c /mnt/d/software/repositories/rm/rmtest_13.cc

CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /mnt/d/software/repositories/rm/rmtest_13.cc > CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.i

CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /mnt/d/software/repositories/rm/rmtest_13.cc -o CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.s

CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o.requires:

.PHONY : CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o.requires

CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o.provides: CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o.requires
	$(MAKE) -f CMakeFiles/rmtest_13.dir/build.make CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o.provides.build
.PHONY : CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o.provides

CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o.provides.build: CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o


# Object files for target rmtest_13
rmtest_13_OBJECTS = \
"CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o"

# External object files for target rmtest_13
rmtest_13_EXTERNAL_OBJECTS =

rmtest_13: CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o
rmtest_13: CMakeFiles/rmtest_13.dir/build.make
rmtest_13: libRM.a
rmtest_13: libRBFM.a
rmtest_13: libPFM.a
rmtest_13: CMakeFiles/rmtest_13.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/mnt/d/software/repositories/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable rmtest_13"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/rmtest_13.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/rmtest_13.dir/build: rmtest_13

.PHONY : CMakeFiles/rmtest_13.dir/build

CMakeFiles/rmtest_13.dir/requires: CMakeFiles/rmtest_13.dir/rm/rmtest_13.cc.o.requires

.PHONY : CMakeFiles/rmtest_13.dir/requires

CMakeFiles/rmtest_13.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/rmtest_13.dir/cmake_clean.cmake
.PHONY : CMakeFiles/rmtest_13.dir/clean

CMakeFiles/rmtest_13.dir/depend:
	cd /mnt/d/software/repositories/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/d/software/repositories /mnt/d/software/repositories /mnt/d/software/repositories/cmake-build-debug /mnt/d/software/repositories/cmake-build-debug /mnt/d/software/repositories/cmake-build-debug/CMakeFiles/rmtest_13.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/rmtest_13.dir/depend

