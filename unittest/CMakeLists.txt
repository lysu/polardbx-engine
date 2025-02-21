# Copyright (c) 2010, 2022, Oracle and/or its affiliates. Copyright (c) 2023, 2024, Alibaba and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA 

ADD_WSHADOW_WARNING()

ADD_SUBDIRECTORY(gunit)

INCLUDE(ProcessorCount)
ProcessorCount(CORES)
IF (CORES EQUAL 0)
  SET(CORES 1)
ENDIF()

set(CTEST_BIN ctest)
if (DEFINED CTEST_BIN_PATH)
  set(CTEST_BIN ${CTEST_BIN_PATH})
endif()

set(CTEST_EXTRA_OPTS "")
if (DEFINED CTEST_XML_OUTPUT_DIRECTORY)
  set(CTEST_EXTRA_OPTS "--output-junit ${CTEST_XML_OUTPUT_DIRECTORY}/${CTEST_XML_OUTPUT_FILE}")
endif()

ADD_CUSTOM_TARGET(
  test-unit
  COMMAND ${CTEST_BIN} --progress --parallel ${CORES} --output-on-failure --timeout=300 ${CTEST_EXTRA_OPTS}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL
)
