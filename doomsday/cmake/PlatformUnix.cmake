# Linux / BSD / other Unix
include (PlatformGenericUnix)

set (DENG_PLATFORM_SUFFIX x11)
set (DENG_X11 ON)

add_definitions (
    -DDENG_X11
    -D__USE_BSD 
    -D_GNU_SOURCE=1
    -DDENG_BASE_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_DATA_DIR}"
    -DDENG_LIBRARY_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_PLUGIN_DIR}"
)

