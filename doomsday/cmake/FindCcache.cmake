find_program (CCACHE_PATH ccache)

if (NOT CCACHE_PATH STREQUAL "CCACHE_PATH-NOTFOUND")
    set (CCACHE_FOUND YES)
    set (CCACHE_OPTION_DEFAULT ON)
else ()
    set (CCACHE_FOUND NO)
    set (CCACHE_OPTION_DEFAULT OFF)
endif ()

option (DENG_ENABLE_CCACHE "Use ccache when compiling" ${CCACHE_OPTION_DEFAULT})

if (DENG_ENABLE_CCACHE)
    set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK ccache) 
endif ()

message (STATUS "Use ccache: ${DENG_ENABLE_CCACHE}")
