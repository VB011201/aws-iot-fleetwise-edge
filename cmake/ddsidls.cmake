# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.

# Generate C++ code from the DDS IDLs, required to interact with 
# devices connected to FWE via a DDS backbone.
# The IDLs are part of a git submodule and this cmake file
# invokes the fastDDS Gen tool to generate the sources locally. 
# These are the library names and aliases

# FastDDS dependencies
find_package(fastrtps REQUIRED)
find_package(fastcdr REQUIRED)

set(libraryTargetName iotfleetwise.idls)
set(libraryAliasName IoTFleetWise::Idls)

# This directory will contain idls files and autogenerated source and headers
set(idlsDir ${CMAKE_BINARY_DIR}/idls/)

set(idlsRoot "${CMAKE_CURRENT_SOURCE_DIR}/interfaces/fastdds/idl")
message(STATUS "IDLs root is: ${idlsRoot}")

# Make a list of all the idl files
file(GLOB_RECURSE idlFilesExternal FOLLOW_SYMLINKS ${idlsRoot}/*.idl)

# Copy the idl files to the build tree. 
file(COPY ${idlFilesExternal} DESTINATION ${idlsDir})

foreach(idlFileName ${idlFilesExternal})
  get_filename_component(idlFile ${idlFileName} NAME)

  message(STATUS "File name is: ${idlFile}")

  if(idlFile)
    message(STATUS "IDL file found.")
  else()
    message(FATAL_ERROR "IDL file not found.")
  endif()
  # Invoke the fastddsgen command
  execute_process(
  COMMAND fastddsgen -d ${idlsDir} ${idlFile}
  WORKING_DIRECTORY ${idlsDir})

endforeach()
# Find the sources and the headers
FILE(GLOB sources ${idlsDir}/*.c*)
FILE(GLOB headers ${idlsDir}/*.h)

# Create the target with the given sources and headers
add_custom_target(idlGenTarget DEPENDS ${sources} ${headers})

add_library(
  ${libraryTargetName}
  ${sources}
)

add_dependencies(${libraryTargetName} idlGenTarget)

target_link_libraries(
  ${libraryTargetName}
  PUBLIC
  fastrtps
  fastcdr
)
target_include_directories(
  ${libraryTargetName}
  PUBLIC
  ${idlsDir}
)
set_target_properties(${libraryTargetName} PROPERTIES LINKER_LANGUAGE CXX)
add_library(${libraryAliasName} ALIAS ${libraryTargetName})
