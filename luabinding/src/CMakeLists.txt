#
# Copyright (c) 2014-2022 Enrico M. Crisostomo
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.
#
set(SOURCE_FILES
        fswatch_lua.c
        ${PROJECT_BINARY_DIR}/libfswatch/libfswatch_config.h)

if(APPLE)
    # The "-undefined dynamic_lookup" is a hack for systems with
    # multiple Python installed. If we link a particular Python version
    # here, and we import it with a different Python version later.
    # likely a segmentation fault.
    # The solution for Linux Mac OS machines, as mentioned in 
    # https://github.com/pybind/pybind11/blob/master/tools/pybind11Tools.cmake
    # is to not link against Python library at all and resolve the symbols
    # at compile time.
    set(DYNAMIC_LOOKUP "-undefined dynamic_lookup")
endif()
add_library(fswatch_lua SHARED ${SOURCE_FILES})
target_include_directories(fswatch_lua PRIVATE ../.. .)
target_include_directories(fswatch_lua PRIVATE ../../../lua)
target_include_directories(fswatch_lua PRIVATE ../../../../skynet-src)
target_include_directories(fswatch_lua PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(fswatch_lua LINK_PUBLIC ${DYNAMIC_LOOKUP} libfswatch)