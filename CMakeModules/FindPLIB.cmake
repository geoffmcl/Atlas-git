# Locate PLIB
# This module defines
# PLIB_LIBRARIES
# PLIB_FOUND, if false, do not try to link to PLIB
# PLIB_INCLUDE_DIR, where to find the headers
#
# $PLIBDIR is an environment variable that would
# correspond to the ./configure --prefix=$PLIBDIR
# used in building PLIB.
#
# set `PLIB_CMAKE_DEBUG` to `ON`, to output debug information
#

include(SelectLibraryConfigurations)

set(save_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK})
set(CMAKE_FIND_FRAMEWORK ONLY)
FIND_PATH(PLIB_INCLUDE_DIR ul.h
  PATH_SUFFIXES include/plib include
  PATHS ${ADDITIONAL_LIBRARY_PATHS}
)
set(CMAKE_FIND_FRAMEWORK ${save_FIND_FRAMEWORK})

if(NOT PLIB_INCLUDE_DIR)
    FIND_PATH(PLIB_INCLUDE_DIR plib/ul.h
      PATH_SUFFIXES include
      HINTS $ENV{PLIBDIR}
      PATHS ${ADDITIONAL_LIBRARY_PATHS}
    )
endif()

if(PLIB_CMAKE_DEBUG)
message(STATUS ${PLIB_INCLUDE_DIR})
endif()

# check for dynamic framework on Mac ()
if(APPLE)
FIND_LIBRARY(PLIB_LIBRARIES
  NAMES plib PLIB
  HINTS
  $ENV{PLIBDIR}
  PATHS ${ADDITIONAL_LIBRARY_PATHS}
)
endif()

if(MSVC)
   set (PUNAME "pui")
else()
   set (PUNAME "pu")
endif()


macro(find_static_component comp libs)
    # account for alternative Windows PLIB distribution naming
    if(MSVC)
      set(compLib "${comp}")
    else()
      set(compLib "plib${comp}")
    endif()
    
    if(PLIB_CMAKE_DEBUG)
        message(STATUS "Finding ${compLib}...")
    endif()

    string(TOUPPER "PLIB_${comp}" compLibBase)
    set( compLibName ${compLibBase}_LIBRARY )

    FIND_LIBRARY(${compLibName}_DEBUG
      NAMES ${compLib}_d plib_${compLib}_d
      HINTS $ENV{PLIBDIR}
      PATH_SUFFIXES lib64 lib libs64 libs libs/Win32 libs/Win64
      PATHS ${ADDITIONAL_LIBRARY_PATHS}
    )
    FIND_LIBRARY(${compLibName}_RELEASE
      NAMES ${compLib} plib_${compLib}
      HINTS $ENV{PLIBDIR}
      PATH_SUFFIXES lib64 lib libs64 libs libs/Win32 libs/Win64
      PATHS ${ADDITIONAL_LIBRARY_PATHS}
    )
    select_library_configurations( ${compLibBase} )

    set(componentLibRelease ${${compLibName}_RELEASE})
    if(PLIB_CMAKE_DEBUG)
        message(STATUS "*** RELEASE ${compLibName}_RELEASE ${componentLibRelease}")
    endif()
    set(componentLibDebug ${${compLibName}_DEBUG})
    if(PLIB_CMAKE_DEBUG)
        message(STATUS "*** DEBUG   ${compLibName}_DEBUG ${componentLibDebug}")
    endif()
    if (NOT ${compLibName}_DEBUG)
        if (${compLibName}_RELEASE)
            if(PLIB_CMAKE_DEBUG)
                message(STATUS "*** found ${componentLib}")
            endif()
            list(APPEND ${libs} ${componentLibRelease})
        endif()
    else()
        list(APPEND ${libs} optimized ${componentLibRelease} debug ${componentLibDebug})
    endif()
endmacro()

if(NOT PLIB_LIBRARIES)
    set(PLIB_LIBRARIES "") # clear value

# based on the contents of deps, add other required PLIB
# static library dependencies. Eg PUI requires FNT
    set(outDeps ${PLIB_FIND_COMPONENTS})

    foreach(c ${PLIB_FIND_COMPONENTS})
        if (${c} STREQUAL "pu")
            # handle MSVC confusion over pu/pui naming, by removing
            # 'pu' and then adding it back
            list(REMOVE_ITEM outDeps "pu" "fnt" "sg")
            list(APPEND outDeps ${PUNAME} "sg")
        elseif (${c} STREQUAL "puaux")
            list(APPEND outDeps ${PUNAME} "sg")
        elseif (${c} STREQUAL "ssg")
            list(APPEND outDeps "sg")
        endif()
    endforeach()

    list(APPEND outDeps "ul") # everything needs ul
    list(REMOVE_DUPLICATES outDeps) # clean up


    # look for traditional static libraries
    foreach(component ${outDeps})
        if(PLIB_CMAKE_DEBUG)
            message(STATUS "*** Finding PLIB component ${component}")
        endif()
        find_static_component(${component} PLIB_LIBRARIES)
    endforeach()
endif()

if(EXISTS "${PLIB_INCLUDE_DIR}/plib/ul.h")
    file(READ "${PLIB_INCLUDE_DIR}/plib/ul.h" ver)
    string(REGEX MATCH "PLIB_MAJOR_VERSION[ ]+([0-9]*)" _ ${ver})
    set(ver_major ${CMAKE_MATCH_1})
    string(REGEX MATCH "PLIB_MINOR_VERSION[ ]+([0-9]*)" _ ${ver})
    set(ver_minor ${CMAKE_MATCH_1})
    string(REGEX MATCH "PLIB_TINY_VERSION[ ]+([0-9]*)" _ ${ver})
    set(ver_tiny ${CMAKE_MATCH_1})
    set(PLIB_VERSION_STR "${ver_major}.${ver_minor}.${ver_tiny}")
endif()


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PLIB DEFAULT_MSG PLIB_LIBRARIES PLIB_INCLUDE_DIR)
