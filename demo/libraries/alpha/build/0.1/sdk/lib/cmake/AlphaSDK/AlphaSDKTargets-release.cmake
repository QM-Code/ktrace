#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "alpha::sdk" for configuration "Release"
set_property(TARGET alpha::sdk APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(alpha::sdk PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libalpha.a"
  )

list(APPEND _cmake_import_check_targets alpha::sdk )
list(APPEND _cmake_import_check_files_for_alpha::sdk "${_IMPORT_PREFIX}/lib/libalpha.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
