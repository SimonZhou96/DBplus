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
include CMakeFiles/rbftest_p2b.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/rbftest_p2b.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/rbftest_p2b.dir/flags.make

CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o: CMakeFiles/rbftest_p2b.dir/flags.make
CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o: ../rbf/rbftest_p2b.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/d/software/repositories/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o -c /mnt/d/software/repositories/rbf/rbftest_p2b.cc

CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /mnt/d/software/repositories/rbf/rbftest_p2b.cc > CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.i

CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /mnt/d/software/repositories/rbf/rbftest_p2b.cc -o CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.s

CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o.requires:

.PHONY : CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o.requires

CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o.provides: CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o.requires
	$(MAKE) -f CMakeFiles/rbftest_p2b.dir/build.make CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o.provides.build
.PHONY : CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o.provides

CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o.provides.build: CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o


# Object files for target rbftest_p2b
rbftest_p2b_OBJECTS = \
"CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o"

# External object files for target rbftest_p2b
rbftest_p2b_EXTERNAL_OBJECTS =

rbftest_p2b: CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o
rbftest_p2b: CMakeFiles/rbftest_p2b.dir/build.make
rbftest_p2b: libRBFM.a
rbftest_p2b: libPFM.a
rbftest_p2b: CMakeFiles/rbftest_p2b.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/mnt/d/software/repositories/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable rbftest_p2b"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/rbftest_p2b.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/rbftest_p2b.dir/build: rbftest_p2b

.PHONY : CMakeFiles/rbftest_p2b.dir/build

CMakeFiles/rbftest_p2b.dir/requires: CMakeFiles/rbftest_p2b.dir/rbf/rbftest_p2b.cc.o.requires

.PHONY : CMakeFiles/rbftest_p2b.dir/requires

CMakeFiles/rbftest_p2b.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/rbftest_p2b.dir/cmake_clean.cmake
.PHONY : CMakeFiles/rbftest_p2b.dir/clean

CMakeFiles/rbftest_p2b.dir/depend:
	cd /mnt/d/software/repositories/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/d/software/repositories /mnt/d/software/repositories /mnt/d/software/repositories/cmake-build-debug /mnt/d/software/repositories/cmake-build-debug /mnt/d/software/repositories/cmake-build-debug/CMakeFiles/rbftest_p2b.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/rbftest_p2b.dir/depend

