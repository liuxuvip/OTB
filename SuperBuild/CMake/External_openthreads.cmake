INCLUDE_ONCE_MACRO(OPENTHREADS)

SETUP_SUPERBUILD(OPENTHREADS)

ExternalProject_Add(OPENTHREADS
  PREFIX OPENTHREADS
  URL "http://www.openscenegraph.org/downloads/stable_releases/OpenSceneGraph-3.4.0/source/OpenSceneGraph-3.4.0.zip"
  URL_MD5 a5e762c64373a46932e444f6f7332496
  SOURCE_DIR ${OPENTHREADS_SB_SRC}
  BINARY_DIR ${OPENTHREADS_SB_BUILD_DIR}
  INSTALL_DIR ${SB_INSTALL_PREFIX}
  DOWNLOAD_DIR ${DOWNLOAD_LOCATION}
  CMAKE_CACHE_ARGS
  ${SB_CMAKE_CACHE_ARGS}
  CMAKE_COMMAND ${SB_CMAKE_COMMAND}
  DEPENDS ${OPENTHREADS_DEPENDENCIES}
  PATCH_COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_SOURCE_DIR}/patches/OPENTHREADS/CMakeLists.txt
  ${OPENTHREADS_SB_SRC}
  )

set(_SB_OPENTHREADS_INCLUDE_DIR ${SB_INSTALL_PREFIX}/include)
if(WIN32)
  set(_SB_OPENTHREADS_LIBRARY ${SB_INSTALL_PREFIX}/lib/OpenThreads.lib)
elseif(UNIX)
  set(_SB_OPENTHREADS_LIBRARY ${SB_INSTALL_PREFIX}/lib/libOpenThreads${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()
