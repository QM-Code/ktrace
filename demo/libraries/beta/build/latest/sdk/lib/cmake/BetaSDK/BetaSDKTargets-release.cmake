#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "beta::sdk" for configuration "Release"
set_property(TARGET beta::sdk APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(beta::sdk PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libbeta.a"
  )

list(APPEND _cmake_import_check_targets beta::sdk )
list(APPEND _cmake_import_check_files_for_beta::sdk "${_IMPORT_PREFIX}/lib/libbeta.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
