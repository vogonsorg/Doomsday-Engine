message (STATUS "Looking for glbinding...")

if (APPLE)
    # Homebrew's glbinding CMake config doesn't seem to work.
    include (${glbinding_DIR}/cmake/glbinding/glbinding-export.cmake)
else ()
    # Use the glbinding built by build_deps.py.
    include (${DE_DEPENDS_DIR}/products/glbinding-config.cmake)
    set (glbinding_DIR ${DE_DEPENDS_DIR}/products)
endif ()

if (MSYS OR WIN32)
   set (DE_GLBINDING_VERSION 3)
else ()
   set (DE_GLBINDING_VERSION 2)
endif ()
