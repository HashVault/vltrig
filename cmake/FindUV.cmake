find_path(
    UV_INCLUDE_DIR
    NAMES uv.h
    PATHS "${XMRIG_DEPS}" ENV "XMRIG_DEPS"
    PATH_SUFFIXES "include"
    NO_DEFAULT_PATH
)

find_path(UV_INCLUDE_DIR NAMES uv.h)

if (BUILD_STATIC)
    set(UV_LIB_NAMES libuv.a libuv_a.a uv libuv)
else()
    set(UV_LIB_NAMES uv libuv libuv.a libuv_a.a)
endif()

find_library(
    UV_LIBRARY
    NAMES ${UV_LIB_NAMES}
    PATHS "${XMRIG_DEPS}" ENV "XMRIG_DEPS"
    PATH_SUFFIXES "lib"
    NO_DEFAULT_PATH
)

find_library(UV_LIBRARY NAMES ${UV_LIB_NAMES})

set(UV_LIBRARIES ${UV_LIBRARY})
set(UV_INCLUDE_DIRS ${UV_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UV DEFAULT_MSG UV_LIBRARY UV_INCLUDE_DIR)
