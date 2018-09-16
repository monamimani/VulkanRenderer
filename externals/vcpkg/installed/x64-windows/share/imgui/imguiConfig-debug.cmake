#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "imgui::imgui" for configuration "Debug"
set_property(TARGET imgui::imgui APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(imgui::imgui PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/imguid.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/imguid.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS imgui::imgui )
list(APPEND _IMPORT_CHECK_FILES_FOR_imgui::imgui "${_IMPORT_PREFIX}/debug/lib/imguid.lib" "${_IMPORT_PREFIX}/debug/bin/imguid.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
