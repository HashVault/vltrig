find_path(
    NGHTTP2_INCLUDE_DIR
    NAMES nghttp2/nghttp2.h
    PATHS "${XMRIG_DEPS}" ENV "XMRIG_DEPS"
    PATH_SUFFIXES "include"
    NO_DEFAULT_PATH
)

find_path(NGHTTP2_INCLUDE_DIR NAMES nghttp2/nghttp2.h)

find_library(
    NGHTTP2_LIBRARY
    NAMES libnghttp2.a nghttp2 libnghttp2
    PATHS "${XMRIG_DEPS}" ENV "XMRIG_DEPS"
    PATH_SUFFIXES "lib"
    NO_DEFAULT_PATH
)

find_library(NGHTTP2_LIBRARY NAMES libnghttp2.a nghttp2 libnghttp2)

set(NGHTTP2_LIBRARIES ${NGHTTP2_LIBRARY})
set(NGHTTP2_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NGHTTP2 DEFAULT_MSG NGHTTP2_LIBRARY NGHTTP2_INCLUDE_DIR)
