cmake_minimum_required(VERSION 3.20)
project(libISW VERSION 1.0.0 LANGUAGES C)

# Default install prefix to /usr (overridable with -DCMAKE_INSTALL_PREFIX=...)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckTypeSize)
include(FindPkgConfig)

# ===========================================================================
# Options
# ===========================================================================

option(ISW_INTERNATIONALIZATION "Enable UTF-8 multibyte support" ON)
option(ISW_HAS_XIM "Enable X Input Method for CJK input (requires Xlib XIM APIs)" OFF)

option(ISW_GRAY_BLKWHT_STIPPLES "Enable gray stipple patterns" OFF)

# ===========================================================================
# Dependencies (pkg-config)
# ===========================================================================

pkg_check_modules(XT_DEPS REQUIRED IMPORTED_TARGET
    sm ice xproto xcb xcb-xrm xcb-keysyms)

pkg_check_modules(ISW_DEPS REQUIRED IMPORTED_TARGET
    xcb xcb-xfixes xcb-shape xcb-present)

pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET
    cairo>=1.12.0 cairo-xcb>=1.12.0)

# Optional: Cairo-EGL
pkg_check_modules(EGL IMPORTED_TARGET egl>=1.4)
pkg_check_modules(CAIRO_GL IMPORTED_TARGET cairo-gl>=1.12.0)

if(EGL_FOUND AND CAIRO_GL_FOUND)
    set(HAVE_CAIRO_EGL TRUE)
else()
    set(HAVE_CAIRO_EGL FALSE)
    message(STATUS "Cairo-EGL not available (missing EGL or cairo-gl)")
endif()

# Optional: xkbcommon
pkg_check_modules(XKBCOMMON IMPORTED_TARGET xkbcommon)


# ===========================================================================
# Header / function checks → config.h
# ===========================================================================

check_include_file(wctype.h HAVE_WCTYPE_H)
check_include_file(wchar.h HAVE_WCHAR_H)
check_include_file(widec.h HAVE_WIDEC_H)
check_function_exists(iswspace HAVE_ISWSPACE)
check_type_size(intptr_t INTPTR_T)

configure_file(config.h.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# ===========================================================================
# Code generation: makestrs
# ===========================================================================

add_executable(makestrs util/makestrs.c)

set(STRINGDEFS_C ${CMAKE_CURRENT_BINARY_DIR}/src/StringDefs.c)
set(STRINGDEFS_H ${CMAKE_CURRENT_BINARY_DIR}/include/X11/StringDefs.h)
set(SHELL_H      ${CMAKE_CURRENT_BINARY_DIR}/include/X11/Shell.h)

# makestrs writes StringDefs.c to stdout, and drops StringDefs.h + Shell.h
# in the current working directory as side effects.
add_custom_command(
    OUTPUT  ${STRINGDEFS_C} ${STRINGDEFS_H} ${SHELL_H}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/src
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include/X11
    COMMAND $<TARGET_FILE:makestrs> -i ${CMAKE_SOURCE_DIR} < ${CMAKE_SOURCE_DIR}/util/string.list
            > ${STRINGDEFS_C}
    COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_CURRENT_BINARY_DIR}/StringDefs.h ${STRINGDEFS_H}
    COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_CURRENT_BINARY_DIR}/Shell.h ${SHELL_H}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS makestrs ${CMAKE_SOURCE_DIR}/util/string.list
    COMMENT "Generating StringDefs.c, StringDefs.h, Shell.h"
)

add_custom_target(generate_strings DEPENDS ${STRINGDEFS_C} ${STRINGDEFS_H} ${SHELL_H})

# ===========================================================================
# Bison / Flex: use pre-generated sources from the source tree
# ===========================================================================
# laygram.c, laygram.h, laylex.c are pre-generated and checked in.
# Regeneration from laygram.y / laylex.l is only needed if those grammars
# change — handled manually, matching the autotools behaviour.

# ===========================================================================
# Source lists
# ===========================================================================

set(LIBXT_SOURCES
    src/ActionHook.c
    src/Alloc.c
    src/ArgList.c
    src/Callback.c
    src/ClickTime.c
    src/Composite.c
    src/Constraint.c
    src/Context.c
    src/Convert.c
    src/Converters.c
    src/Core.c
    src/Create.c
    src/Destroy.c
    src/Display.c
    src/Error.c
    src/Event.c
    src/EventUtil.c
    src/Functions.c
    src/GCManager.c
    src/Geometry.c
    src/GetActKey.c
    src/GetResList.c
    src/GetValues.c
    src/HookObj.c
    src/Hooks.c
    src/Initialize.c
    src/Intrinsic.c
    src/Keyboard.c
    src/Manage.c
    src/NextEvent.c
    src/Object.c
    src/PassivGrab.c
    src/Pointer.c
    src/Popup.c
    src/PopupCB.c
    src/Quark.c
    src/RectObj.c
    src/ResConfig.c
    src/Resources.c
    src/Selection.c
    src/SetSens.c
    src/SetValues.c
    src/SetWMCW.c
    src/Shell.c
    src/Threads.c
    src/TMaction.c
    src/TMgrab.c
    src/TMkey.c
    src/TMparse.c
    src/TMprint.c
    src/TMstate.c
    src/VarCreate.c
    src/VarGet.c
    src/Varargs.c
)

set(ISW_WIDGET_SOURCES
    src/AllWidgets.c
    src/AsciiSink.c
    src/AsciiSrc.c
    src/AsciiText.c
    src/Box.c
    src/ColorPicker.c
    src/ComboBox.c
    src/Command.c
    src/DrawingArea.c
    src/Dialog.c
    src/FlexBox.c
    src/FontChooser.c
    src/Form.c
    src/Grip.c
    src/IconView.c
    src/Label.c
    src/Layout.c
    src/List.c
    src/MainWindow.c
    src/MenuBar.c
    src/MenuButton.c
    src/Paned.c
    src/Panner.c
    src/Porthole.c
    src/ProgressBar.c
    src/Repeater.c
    src/Scale.c
    src/Scrollbar.c
    src/ScrollWheel.c
    src/SpinBox.c
    src/StatusBar.c
    src/Simple.c
    src/SimpleMenu.c
    src/SmeBSB.c
    src/Sme.c
    src/SmeLine.c
    src/StripChart.c
    src/Tabs.c
    src/TextAction.c
    src/Text.c
    src/TextPop.c
    src/TextSink.c
    src/TextSrc.c
    src/TextTr.c
    src/Tip.c
    src/Toolbar.c
    src/Toggle.c
    src/Tree.c
    src/Vendor.c
    src/Viewport.c
    src/ISWP.c
    src/ISWAtoms.c
    src/ISWContext.c
    src/ISWDrawing.c
    src/ISWInit.c
    src/ISWXcbDraw.c
    src/ISWXdnd.c
    src/Image.c
    src/ISWImage.c
)

set(RENDER_SOURCES
    src/ISWRender.c
    src/ISWRenderCairoXCB.c
)

if(HAVE_CAIRO_EGL)
    list(APPEND RENDER_SOURCES src/ISWRenderEGL.c)
endif()

set(SVG_SOURCES
    src/nanosvg_impl.c
    src/ISWSVG.c
    src/lodepng.c
    src/ISWPNG.c
)

set(I18N_SOURCES)
if(ISW_INTERNATIONALIZATION)
    set(I18N_SOURCES
        src/MultiSrc.c
        src/MultiSink.c
        src/ISWIm.c
        src/ISWI18n.c
    )
endif()

# ===========================================================================
# Library target
# ===========================================================================

add_library(ISW SHARED
    ${LIBXT_SOURCES}
    ${ISW_WIDGET_SOURCES}
    ${RENDER_SOURCES}
    ${SVG_SOURCES}
    ${I18N_SOURCES}
    ${STRINGDEFS_C}
    src/laygram.c
    src/laylex.c
)

add_dependencies(ISW generate_strings)

set_target_properties(ISW PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION 1
    C_STANDARD 11
    OUTPUT_NAME ISW
)

# Include paths
target_include_directories(ISW
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include/X11
        ${CMAKE_CURRENT_BINARY_DIR}/include/X11
        ${CMAKE_CURRENT_BINARY_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Compile definitions
target_compile_definitions(ISW PRIVATE
    LIBXT_COMPILATION
    _CONST_X_STRING=1
    ISW_ARROW_SCROLLBARS
    "XFILESEARCHPATHDEFAULT=\"${CMAKE_INSTALL_FULL_DATADIR}/X11/%L/%T/%N%C%S:${CMAKE_INSTALL_FULL_DATADIR}/X11/%T/%N%C%S\""
    "ERRORDB=\"${CMAKE_INSTALL_FULL_DATADIR}/X11/XtErrorDB\""
    HAVE_CONFIG_H
    HAVE_CAIRO=1
    HAVE_CAIRO_XCB=1
)

if(ISW_INTERNATIONALIZATION)
    target_compile_definitions(ISW PRIVATE ISW_INTERNATIONALIZATION)
endif()

if(ISW_HAS_XIM)
    target_compile_definitions(ISW PRIVATE ISW_HAS_XIM)
endif()


if(ISW_GRAY_BLKWHT_STIPPLES)
    target_compile_definitions(ISW PRIVATE ISW_GRAY_BLKWHT_STIPPLES)
endif()

if(HAVE_CAIRO_EGL)
    target_compile_definitions(ISW PRIVATE HAVE_CAIRO_EGL=1)
endif()

if(XKBCOMMON_FOUND)
    target_compile_definitions(ISW PRIVATE HAVE_XKBCOMMON=1)
endif()

# Warning flags (replaces XORG_DEFAULT_OPTIONS)
target_compile_options(ISW PRIVATE
    -Wall
    -Wpointer-arith
    -Wmissing-declarations
    -Wformat=2
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wnested-externs
    -Wbad-function-cast
    -Wold-style-definition
    -Wunused
    -Wuninitialized
    -Wshadow
    -Wredundant-decls
    -fno-strict-aliasing
    -Werror=implicit
    -Werror=nonnull
    -Werror=init-self
    -Werror=main
    -Werror=missing-braces
    -Werror=sequence-point
    -Werror=return-type
    -Werror=trigraphs
    -Werror=array-bounds
    -Werror=write-strings
    -Werror=address
    -Werror=int-to-pointer-cast
    -Werror=pointer-to-int-cast
)

# Link libraries
target_link_libraries(ISW PRIVATE
    PkgConfig::XT_DEPS
    PkgConfig::ISW_DEPS
    PkgConfig::FONTCONFIG
    PkgConfig::FREETYPE
    PkgConfig::CAIRO
    m
)

if(HAVE_CAIRO_EGL)
    target_link_libraries(ISW PRIVATE PkgConfig::EGL PkgConfig::CAIRO_GL)
endif()

if(XKBCOMMON_FOUND)
    target_link_libraries(ISW PRIVATE PkgConfig::XKBCOMMON)
endif()


# ===========================================================================
# Demo executable (not installed)
# ===========================================================================

add_executable(isw_demo EXCLUDE_FROM_ALL
    examples/isw_demo.c
    examples/multi_stubs.c
)

pkg_check_modules(DEMO_DEPS REQUIRED IMPORTED_TARGET
    xcb xcb-icccm xcb-xfixes xcb-render xcb-shape)

target_link_libraries(isw_demo PRIVATE ISW PkgConfig::DEMO_DEPS m)

target_include_directories(isw_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_custom_target(demo
    COMMAND ${CMAKE_COMMAND} -E env
        "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:ISW>:$ENV{LD_LIBRARY_PATH}"
        $<TARGET_FILE:isw_demo>
    DEPENDS isw_demo
    COMMENT "Running ISW demo"
)

# ===========================================================================
# pkg-config
# ===========================================================================

configure_file(
    ${CMAKE_SOURCE_DIR}/isw.pc.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/isw.pc
    @ONLY
)

# ===========================================================================
# Install
# ===========================================================================

install(TARGETS ISW
    EXPORT ISWTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(EXPORT ISWTargets
    FILE ISWTargets.cmake
    NAMESPACE ISW::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ISW
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/ISWConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ISWConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ISW
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ISWConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ISWConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ISWConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ISW
)

# ISW widget headers
install(DIRECTORY include/ISW/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ISW
    FILES_MATCHING PATTERN "*.h"
    PATTERN "Template.h" EXCLUDE
    PATTERN "TemplateP.h" EXCLUDE
)

# Conditional I18N headers are in the same directory, so they're picked up
# automatically. Exclude them when I18N is off.
if(NOT ISW_INTERNATIONALIZATION)
    # Remove I18N headers from install via explicit install override
    install(CODE "
        file(REMOVE
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/ISW/MultiSrc.h\"
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/ISW/MultiSrcP.h\"
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/ISW/MultiSink.h\"
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/ISW/MultiSinkP.h\"
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/ISW/VendorEP.h\"
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/ISW/ISWImP.h\"
        )
    ")
endif()

# X Toolkit Intrinsics headers
install(DIRECTORY include/X11/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/X11
    FILES_MATCHING PATTERN "*.h"
)

# Generated headers
install(FILES ${STRINGDEFS_H} ${SHELL_H}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/X11
)

# uthash
install(FILES include/uthash.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# pkg-config file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/isw.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "==========================================")
message(STATUS "libISW Configuration Summary")
message(STATUS "==========================================")
message(STATUS "Internationalization:    ${ISW_INTERNATIONALIZATION}")
message(STATUS "XIM support:             ${ISW_HAS_XIM}")
message(STATUS "")
message(STATUS "Rendering Backends:")
message(STATUS "  Cairo-XCB:             yes")
message(STATUS "  Cairo-EGL:             ${HAVE_CAIRO_EGL}")
message(STATUS "")
message(STATUS "Optional:")
message(STATUS "  xkbcommon:             ${XKBCOMMON_FOUND}")

message(STATUS "==========================================")
message(STATUS "")
