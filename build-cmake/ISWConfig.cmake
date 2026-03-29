
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was ISWConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

find_dependency(PkgConfig)
pkg_check_modules(ISW_SM_ICE REQUIRED IMPORTED_TARGET sm ice)
pkg_check_modules(ISW_XCB REQUIRED IMPORTED_TARGET
    xcb xcb-xrm xcb-keysyms xcb-xfixes xcb-shape)
pkg_check_modules(ISW_FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
pkg_check_modules(ISW_FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(ISW_CAIRO REQUIRED IMPORTED_TARGET cairo>=1.12.0 cairo-xcb>=1.12.0)

include("${CMAKE_CURRENT_LIST_DIR}/ISWTargets.cmake")

check_required_components(ISW)
