# Appends a string to an existing variable.
macro (append var str)
    if ("${${var}}" STREQUAL "")
        set (${var} "${str}")
    else ()
        set (${var} "${${var}} ${str}")
    endif ()
endmacro (append)

# Returns a subset of a list. If length is -1, the entire remainder
# of the list is returned.
function (sublist outputVariable startIndex length)
    set (_sublist ${ARGV})
    list (REMOVE_AT _sublist 0)
    list (REMOVE_AT _sublist 0)
    # Remove items prior to the start.
    foreach (i RANGE ${startIndex})
        list (REMOVE_AT _sublist 0)
    endforeach (i)
    # Adjust the length.
    if (length GREATER -1)
        list (LENGTH _sublist s)
        while (s GREATER length)
            math (EXPR lastIdx "${s} - 1")
            list (REMOVE_AT _sublist ${lastIdx})
            set (s ${lastIdx})
        endwhile (s GREATER length)
    endif ()
    set (${outputVariable} ${_sublist} PARENT_SCOPE)
endfunction (sublist)

# Removes matching items from the list.
function (list_remove_matches listName expr)
    foreach (item ${${listName}})
        if (NOT item MATCHES ${expr})
            list (APPEND result ${item})
        endif ()
    endforeach (item)
    set (${listName} ${result} PARENT_SCOPE)
endfunction (list_remove_matches)

macro (clean_paths outputVariable text)
    string (REGEX REPLACE "${CMAKE_BINARY_DIR}/([A-Za-z0-9]+)"
        "\\1" ${outputVariable} ${text}
    )
endmacro ()

macro (enable_cxx11 target)
    if (NOT CMAKE_VERSION VERSION_LESS 3.1 AND 
        NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        set_property (TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
        set_property (TARGET ${target} PROPERTY CXX_STANDARD 11)
    else ()
        # This should be unnecessary with CMake 3.2+; the above should be enough.
        append (CMAKE_C_FLAGS   "-std=c11")
        append (CMAKE_CXX_FLAGS "-std=c++11")
    endif ()
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        append (CMAKE_C_FLAGS "-fms-extensions")
    endif ()
endmacro (enable_cxx11)

macro (strict_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set_property (TARGET ${target} 
            APPEND PROPERTY COMPILE_OPTIONS -Wall -Wextra -pedantic
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_property (TARGET ${target} 
            APPEND PROPERTY COMPILE_OPTIONS -Wall -Wextra
        )
    endif ()
endmacro (strict_warnings)

macro (relaxed_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS
                -Wno-missing-field-initializers 
                -Wno-missing-braces
                -Wno-nested-anon-types
                -Wno-gnu-anonymous-struct
                -Wno-deprecated-declarations
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS
                -Wno-missing-field-initializers 
        )
    endif ()
endmacro (relaxed_warnings)

macro (deng_target_rpath target)
    if (APPLE)
        set_target_properties (${target} PROPERTIES
            INSTALL_RPATH "@loader_path/../Frameworks"
        )
        if (${target} MATCHES "test_.*")
            set_property (TARGET ${target} APPEND PROPERTY
                INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_LIB_DIR}"
            )
        endif ()
    elseif (UNIX)
        set_property (TARGET ${target} 
            PROPERTY INSTALL_RPATH                 
                "${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_PLUGIN_DIR};${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_LIB_DIR}"
        )
    endif ()        
endmacro (deng_target_rpath)

macro (deng_target_defaults target)
    if (APPLE)        
        deng_xcode_attribs (${target})
        # OS X version numbers come from the Info.plist, we don't need version symlinks.
    else () 
        set_target_properties (${target} PROPERTIES 
            VERSION   ${DENG_VERSION}
            SOVERSION ${DENG_COMPAT_VERSION}
        )
    endif ()
    deng_target_rpath (${target})
    enable_cxx11 (${target})
    strict_warnings (${target})    
    #cotire (${target})
endmacro (deng_target_defaults)

# Checks all the files in the arguments and removes the ones that
# are not applicable to the current platform.
function (deng_filter_platform_sources outName)
    list (REMOVE_AT ARGV 0) # outName
    foreach (fn ${ARGV})
        set (filtered NO)
        if ("${fn}" MATCHES ".*_windows\\..*" OR 
            "${fn}" MATCHES ".*/windows/.*") # Windows-specific
            if (NOT WIN32)
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_macx\\..*") # OS X specific
            if (NOT APPLE)
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_unix\\..*" OR
                "${fn}" MATCHES ".*/unix/.*") # Unix specific files (Linux / OS X / etc.)
            if (NOT UNIX)
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_x11\\..*") # X11 specific files
            if (APPLE OR NOT UNIX)
                set (filtered YES)
            endif ()
        endif ()
        if (NOT filtered)
            list (APPEND result ${fn})
        endif ()
    endforeach (fn)
    set (${outName} ${result} PARENT_SCOPE)
endfunction (deng_filter_platform_sources)

# Set up resource bundling on OS X.
# The arguments are a list of resource files/directories with the 
# destination directory separated by a comma:
#
#   res/macx/shell.icns,Resources
#
# If the destionation is omitted, it defaults to "Resources".
#
# If the file path is the name of an existing target, its location
# is used as the path.    
#
# DENG_RESOURCES is set to a list of the individual source files
# to be added to add_executable().
#
# deng_bundle_resources() must be called after the target is added
# to specify the locations of the individual files.
function (deng_find_resources)
    string (LENGTH ${CMAKE_CURRENT_SOURCE_DIR} srcDirLen)
    math (EXPR srcDirLen "${srcDirLen} + 1")
    set (src)
    foreach (pair ${ARGV})
        string (REGEX REPLACE "(.*),.*" "\\1" fn ${pair})
        string (REGEX REPLACE ".*,(.*)" "\\1" dest ${pair})
        if (NOT pair MATCHES ".*,.*")
            # No comma means the destination defaults to Resources.
            set (dest Resources)
        endif ()
        if (TARGET ${fn})
            # Use the location of the target.
            get_property (fn TARGET ${fn} PROPERTY LOCATION)
        endif ()
        set (origFn ${fn})
        if (NOT IS_ABSOLUTE ${fn})
            set (fn ${CMAKE_CURRENT_SOURCE_DIR}/${fn})
        endif ()
        if (NOT EXISTS ${fn})
            # Just ignore it.
            message (STATUS "deng_find_resources: Ignoring ${fn} -- not found")
        elseif (NOT IS_DIRECTORY ${fn})
            list (APPEND src ${origFn})
            # Just add as a single file.
            list (APPEND spec "${origFn},${dest}")
        else ()
            #list (APPEND src ${origFn})
            # Do a glob to find all the files.
            file (GLOB_RECURSE _all ${fn}/*)
            # Determine length of the base path since it will be omitted 
            # from destination.
            get_filename_component (baseDir ${fn} DIRECTORY)
            string (LENGTH ${baseDir} baseLen)
            math (EXPR baseLen "${baseLen} + 1")
            foreach (path ${_all})
                get_filename_component (subDir ${path} DIRECTORY)
                string (SUBSTRING ${subDir} ${baseLen} -1 subDest)
                # Omit the current source directory.
                if (path MATCHES "${CMAKE_CURRENT_SOURCE_DIR}.*")
                    string (SUBSTRING ${path} ${srcDirLen} -1 subPath)
                else ()
                    set (subPath ${path})
                endif ()
                list (APPEND spec "${subPath},${dest}/${subDest}")
                list (APPEND src ${subPath})
            endforeach (path)
        endif ()
    endforeach (pair)
    set (_deng_resource_spec ${spec} PARENT_SCOPE)
    set (DENG_RESOURCES ${src} PARENT_SCOPE)
endfunction (deng_find_resources)

# Called after deng_setup_resources() and add_executable() to cause
# the files to be actually placed into the bundle.
function (deng_bundle_resources)
    foreach (pair ${_deng_resource_spec})
        string (REGEX REPLACE "(.*),.*" "\\1" fn ${pair})
        string (REGEX REPLACE ".*,(.*)" "\\1" dest ${pair})
        set_source_files_properties (${fn} PROPERTIES MACOSX_PACKAGE_LOCATION ${dest})        
    endforeach (pair)
endfunction (deng_bundle_resources)

# Build a Doomsday 2 package using the buildpackage.py script.
function (deng_add_package packName)
    set (outName "${packName}.pack")
    get_filename_component (fullPath "${outName}" ABSOLUTE)
    if (NOT EXISTS ${fullPath})
        message (FATAL_ERROR "deng_package: \"${outName}\" not found")
    endif ()
    set (outDir ${CMAKE_CURRENT_BINARY_DIR})
    execute_process (COMMAND ${PYTHON_EXECUTABLE} 
        "${DENG_SOURCE_DIR}/build/scripts/buildpackage.py"
        ${fullPath} ${outDir}
        OUTPUT_VARIABLE msg
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    clean_paths (msg ${msg})
    message (STATUS "${msg}")
    add_custom_target (${packName} ALL DEPENDS ${outName})
    set_target_properties (${packName} PROPERTIES
        LOCATION "${outDir}/${outName}"
        FOLDER Packages
    )
    install (FILES ${outDir}/${outName} DESTINATION ${DENG_INSTALL_DATA_DIR})
    set (DENG_REQUIRED_PACKAGES ${DENG_REQUIRED_PACKAGES} ${packName} PARENT_SCOPE)
endfunction (deng_add_package)

function (deng_find_packages fullPaths)
    set (PKG_DIR "${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_DATA_DIR}")
    sublist (names 1 -1 ${ARGV})
    if (NOT names)
        return ()
    endif ()
    list (REMOVE_DUPLICATES names)
    foreach (name ${names})
        if (TARGET ${name})
            get_property (loc TARGET ${name} PROPERTY LOCATION)            
            list (APPEND result ${loc})
        else ()
            # Check the installed packages.
            set (fn "${PKG_DIR}/${name}.pack")
            if (EXISTS ${fn})
                list (APPEND result ${fn})
            else ()
                message (WARNING "Package \"${name}\" not found")
            endif ()
        endif ()
    endforeach (name)
    set (${fullPaths} ${result} PARENT_SCOPE)
endfunction (deng_find_packages)

macro (deng_add_library target)
    # Form the list of source files.
    sublist (_src 1 -1 ${ARGV})
    deng_filter_platform_sources (_src ${_src})
    # Define the target and namespace alias.
    add_library (${target} SHARED ${_src})
    set (_src)
    add_library (Deng::${target} ALIAS ${target})
    # Libraries use the "deng_" prefix.
    string (REGEX REPLACE "lib(.*)" "deng_\\1" _outName ${target})
    set_target_properties (${target} PROPERTIES 
        OUTPUT_NAME ${_outName}
        FOLDER Libraries
    )
    set (_outName)
    # Compiler settings.
    deng_target_defaults (${target})
    target_include_directories (${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:include/>
    )    
    #cotire (${target})
endmacro (deng_add_library)

macro (deng_deploy_library target name)
    install (TARGETS ${target}
        EXPORT ${name} 
        LIBRARY DESTINATION ${DENG_INSTALL_LIB_DIR}
        INCLUDES DESTINATION include)
    install (EXPORT ${name} DESTINATION lib/cmake/${name} NAMESPACE Deng::)
    install (FILES ${name}Config.cmake DESTINATION lib/cmake/${name})
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/de)
        install (DIRECTORY include/de DESTINATION include)
    endif ()
endmacro (deng_deploy_library)

# Defines a new GUI application target that includes all the required Doomsday
# 2 packages.)
macro (deng_add_application target)
    sublist (src 1 -1 ${ARGV})
    # Check for additional bundle resources.
    list (FIND src EXTRA_RESOURCES idx)
    if (idx GREATER -1)
        math (EXPR pos "${idx} + 1")
        sublist (extraRes ${pos} -1 ${src})
        sublist (src 0 ${idx} ${src})
    endif ()
    deng_find_packages (pkgs ${DENG_REQUIRED_PACKAGES})
    if (APPLE)
        deng_find_resources (${pkgs} ${extraRes})
        add_executable (${target} MACOSX_BUNDLE ${src} ${DENG_RESOURCES})
        deng_bundle_resources ()
        install (TARGETS ${target} DESTINATION .)
        macx_set_bundle_name ("net.dengine.${target}")
        set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE ${target})
    else ()
        add_executable (${target} ${src})
        install (TARGETS ${target} DESTINATION bin)
        install (FILES ${pkgs} DESTINATION ${DENG_INSTALL_DATA_DIR})
    endif ()
    deng_target_defaults (${target})
    set_property (TARGET ${target} PROPERTY FOLDER Apps)
    if (target MATCHES "test_.*")
        if (APPLE)
            # Tests should be runnable from distrib/products.
            set_property (TARGET ${target} APPEND PROPERTY 
                INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_LIB_DIR}"
            )
        endif ()        
    endif ()
    set (src)
    set (pkgs)
    set (idx)
    set (pos)
    set (extraRes)
endmacro (deng_add_application)

function (add_pkgconfig_interface_library target)
    sublist (pkgNames 1 -1 ${ARGV})
    list (GET pkgNames 0 first)
    if (NOT first STREQUAL "OPTIONAL")
        set (checkMode REQUIRED)
    else ()
        list (REMOVE_AT pkgNames 0)        
    endif ()
    foreach (pkg ${pkgNames})
        set (prefix "PKG_${pkg}")
        pkg_check_modules (${prefix} ${checkMode} ${pkg})
        if (NOT checkMode STREQUAL "REQUIRED" AND NOT ${prefix}_FOUND)
            return ()
        endif ()
        # Locate full paths of the required shared libraries.
        foreach (lib ${${prefix}_LIBRARIES})
            find_library (path ${lib} HINTS ${${prefix}_LIBRARY_DIRS})
            get_filename_component (path ${path} REALPATH)
            list (APPEND libs ${path})
            unset (path CACHE)
            set (path)
        endforeach (lib)
        list (APPEND cflags ${${prefix}_CFLAGS})
    endforeach (pkg)
    list (REMOVE_DUPLICATES cflags)
    list (REMOVE_DUPLICATES libs)
    add_library (${target} INTERFACE)
    target_compile_options (${target} INTERFACE ${cflags})
    target_link_libraries (${target} INTERFACE ${libs})
endfunction (add_pkgconfig_interface_library)

function (fix_bundled_install_names binaryFile)
    if (NOT EXISTS ${binaryFile})
        message (FATAL_ERROR "fix_bundled_install_names: ${binaryFile} not found")
    endif ()
    if (binaryFile MATCHES ".*\\.bundle")
        set (ref "@loader_path/../Frameworks")
    else ()
        set (ref "@executable_path/../Frameworks")
    endif ()
    sublist (libs 1 -1 ${ARGV})
    # Check for arguments.
    list (GET libs 0 first)
    if (first STREQUAL "LD_PATH")
        list (GET libs 1 ref)
        list (REMOVE_AT libs 1 0)        
    endif ()
    list (GET libs -1 last)
    if (last STREQUAL "VERBATIM")
        set (verbatim ON)
        list (REMOVE_AT libs -1)
    endif ()
    find_program (OTOOL_EXECUTABLE otool)
    execute_process (COMMAND ${OTOOL_EXECUTABLE} 
        -L ${binaryFile}
        OUTPUT_VARIABLE deps
    )
    foreach (fn ${libs})
        if (NOT verbatim)
            get_filename_component (base "${fn}" NAME)
        else ()
            set (base "${fn}")
        endif ()
        string (REGEX MATCH "([^\n]+/${base}) \\(compatibility version" matched ${deps})
        if (CMAKE_MATCH_1)
            string (STRIP ${CMAKE_MATCH_1} depPath)
            message (STATUS "Changing install name: ${depPath}")
            execute_process (COMMAND ${CMAKE_INSTALL_NAME_TOOL}
                -change "${depPath}" "${ref}/${base}"
                "${binaryFile}"
            )
        endif ()
    endforeach (fn)    
endfunction (fix_bundled_install_names)

# Fixes the install names of the listed libraries that have been bundled into
# the target.
function (deng_bundle_install_names target)
    sublist (libs 1 -1 ${ARGV})
    # Check for arguments.
    list (GET libs 0 first)
    if (first STREQUAL "SCRIPT_NAME")
        list (GET libs 1 _s)
        set (_suffix "-${_s}")
        list (REMOVE_AT libs 0)
        list (REMOVE_AT libs 0)
    endif ()
    set (scriptName "${CMAKE_CURRENT_BINARY_DIR}/postbuild${_suffix}-${target}.cmake")
    # Correct the install names of the dependent libraries.
    file (GENERATE OUTPUT "${scriptName}" CONTENT "\
set (CMAKE_MODULE_PATH ${DENG_SOURCE_DIR}/cmake)\n\
set (CMAKE_INSTALL_NAME_TOOL ${CMAKE_INSTALL_NAME_TOOL})\n\
include (Macros)\n\
fix_bundled_install_names (\"${CMAKE_CURRENT_BINARY_DIR}/${target}.bundle/Contents/MacOS/${target}\"\
    \"${libs}\")\n")
    add_custom_command (TARGET ${target} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -P "${scriptName}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )    
endfunction (deng_bundle_install_names)

# OS X: Install the libraries of a dependency target into the application bundle.
macro (deng_install_bundle_deps target)
    if (APPLE)
        sublist (_deps 1 -1 ${ARGV})
        get_property (_outName TARGET ${target} PROPERTY OUTPUT_NAME)
        set (_fwDir "${CMAKE_INSTALL_PREFIX}/${_outName}.app/Contents/Frameworks")
        foreach (_dep ${_deps})
            if (TARGET ${_dep})
                if (_dep MATCHES "Deng::(.*)")
                    install (FILES $<TARGET_FILE:${_dep}> DESTINATION ${_fwDir})
                else ()
                    install (FILES $<TARGET_PROPERTY:${_dep},INTERFACE_LINK_LIBRARIES>
                        DESTINATION ${_fwDir}
                    )
                endif ()
            endif ()
        endforeach (_dep)
    endif ()
endmacro (deng_install_bundle_deps)

function (deng_install_deployqt target)
    if (UNIX AND NOT APPLE)
        return () # No need to deploy Qt.
    endif ()
    get_property (_outName TARGET ${target} PROPERTY OUTPUT_NAME)
    if (APPLE) # AND NOT DENG_DEVELOPER) # don't run this on a dev build
        if (NOT MACDEPLOYQT_COMMAND)
            message (FATAL_ERROR "macdeployqt not available")
        endif ()
        install (CODE "message (STATUS \"Running macdeployqt on ${_outName}.app...\")
            execute_process (COMMAND ${MACDEPLOYQT_COMMAND} \"${CMAKE_INSTALL_PREFIX}/${_outName}.app\"
                OUTPUT_QUIET ERROR_QUIET)")
    elseif (WIN32)
        # run windeployqt
    endif ()
endfunction (deng_install_deployqt)

function (deng_install_tool target)
    install (TARGETS ${target} DESTINATION bin)
    # OS X: Also install to the client application bundle.
    if (APPLE)
        set (dest "Doomsday.app/Contents/MacOS")
        get_property (name TARGET ${target} PROPERTY OUTPUT_NAME)
        if (NOT name)
            set (name ${target})
        endif()
        install (TARGETS ${target} DESTINATION ${dest})
        install (CODE "
            include (${DENG_SOURCE_DIR}/cmake/Macros.cmake)
            set (CMAKE_INSTALL_NAME_TOOL ${CMAKE_INSTALL_NAME_TOOL})
            fix_bundled_install_names (\"${CMAKE_INSTALL_PREFIX}/${dest}/${name}\"
                QtCore.framework/Versions/5/QtCore
                QtNetwork.framework/Versions/5/QtNetwork
                VERBATIM)
            ")
    endif ()
endfunction (deng_install_tool)

# Install an external library that exists at configuration time.
# Used with dependencies.
macro (deng_install_library library)
    if (UNIX AND NOT APPLE)
        string (REGEX REPLACE "(.*)\\.so" "\\1-*.so" versioned ${library})            
        message ("${library}: ${versioned}")
        file (GLOB _links ${library}.* ${versioned})
        install (FILES ${library} ${_links}
            DESTINATION ${DENG_INSTALL_PLUGIN_DIR}
        )
    endif ()        
endmacro (deng_install_library)

