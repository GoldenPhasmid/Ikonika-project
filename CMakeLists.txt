cmake_minimum_required(VERSION 3.21)

# C is enabled for one reason: the vendored SQLite amalgamation.
project(Ikonika VERSION 0.8.2 LANGUAGES C CXX)

# Play Store requires this to strictly increase on every upload, independent of PROJECT_VERSION.
# Bump it by hand for each Android release.
set(IKONIKA_ANDROID_VERSION_CODE 20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Both compilers are supported deliberately. MSVC is the primary toolchain; MinGW stays viable for
# core and the tests. Nothing outside this block is compiler specific.
if (MSVC)
    # /utf-8 matters here: source and test data contain French accented characters. /FS serialises
    # PDB writes, since MSBuild can run more than one CL.EXE against this target at once and without
    # it they corrupt each other's debug info under a large enough rebuild.
    add_compile_options(/W4 /permissive- /utf-8 /FS)
else()
    # GCC already defaults to UTF-8, but stating it keeps accented string literals byte-identical
    # to the source regardless of the system codepage.
    add_compile_options(-Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

option(IKONIKA_BUILD_APP "Build the Qt Quick application" ON)

if (ANDROID OR IOS)
    set(IKONIKA_DESKTOP OFF)
else()
    set(IKONIKA_DESKTOP ON)
    add_compile_definitions(IKONIKA_DESKTOP)
endif()

# Three of the generated projects are this project's; the rest are Qt's QML tooling, CMake's own
# bookkeeping, and the vendored libraries. Foldering them keeps the solution readable without
# changing what is built - none of these targets can be removed, they are regenerated on every
# configure.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ALL_BUILD, ZERO_CHECK, RUN_TESTS.
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

# Ahead-of-time compilation statistics for the QML compiler. Nothing here reads them, and they
# bring a target per module plus an aggregate.
set(QT_QML_GENERATE_AOTSTATS OFF)

# Qt's own generated targets: autogen, qmlimportscan, qmltyperegistration and the translation
# drivers.
set_property(GLOBAL PROPERTY QT_TARGETS_FOLDER "Qt")
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER "Qt")
set_property(GLOBAL PROPERTY AUTOMOC_TARGETS_FOLDER "Qt")

enable_testing()

# CMAKE_FOLDER applies to every target created while it is set, which is what puts the vendored
# libraries and Catch2 together without naming each one.
set(CMAKE_FOLDER "Third party")
add_subdirectory(third_party/miniaudio)
add_subdirectory(third_party/miniz)
add_subdirectory(third_party/sqlite)

if (IKONIKA_DESKTOP)
    add_subdirectory(third_party/sherpa-onnx)
    add_subdirectory(third_party/onnxruntime)
endif()

unset(CMAKE_FOLDER)

add_subdirectory(core)

if (IKONIKA_DESKTOP)
    add_subdirectory(tests)
endif()

# Off means core and its tests still configure and build on a machine with no Qt, which is the
# point of core having no Qt dependency.
if (IKONIKA_BUILD_APP)
    add_subdirectory(app)
endif()

if (IKONIKA_BUILD_APP AND IKONIKA_DESKTOP)
    # Packaging. See docs/releases.md.

    # The C and C++ runtimes the app was compiled against, so the package does not depend on the
    # user having installed a matching redistributable.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    include(InstallRequiredSystemLibraries)

    set(CPACK_GENERATOR INNOSETUP)
    set(CPACK_PACKAGE_NAME Ikonika)
    set(CPACK_PACKAGE_VENDOR Ikonika)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vocabulary trainer")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY Ikonika)
    set(CPACK_PACKAGE_FILE_NAME "Ikonika-Setup-${PROJECT_VERSION}")

    set(CPACK_PACKAGE_EXECUTABLES IkonikaApp Ikonika)
    set(CPACK_INNOSETUP_CREATE_UNINSTALL_LINK ON)

    set(CPACK_INNOSETUP_INSTALL_ROOT "{autopf}")
    set(CPACK_INNOSETUP_SETUP_PrivilegesRequired lowest)

    # Identifies the existing installation. Changing it installs a second copy alongside.
    set(CPACK_INNOSETUP_SETUP_AppId "{{89146247-DDFE-4522-BB79-05596AD96D7C}")

    set(CPACK_INNOSETUP_SETUP_CloseApplications yes)
    set(CPACK_INNOSETUP_SETUP_RestartApplications yes)
    set(CPACK_INNOSETUP_SETUP_UsePreviousAppDir no)
    set(CPACK_INNOSETUP_RUN_EXECUTABLES IkonikaApp)
    set(CPACK_INNOSETUP_EXTRA_SCRIPTS "${CMAKE_SOURCE_DIR}/packaging/relaunch.iss"
                                      "${CMAKE_SOURCE_DIR}/packaging/uninstall.iss")

    find_program(IKONIKA_ISCC
        NAMES ISCC
        HINTS "$ENV{LOCALAPPDATA}/Programs/Inno Setup 7"
              "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6"
        DOC "The Inno Setup command line compiler, which builds the installer")

    if (IKONIKA_ISCC)
        set(CPACK_INNOSETUP_EXECUTABLE "${IKONIKA_ISCC}")
    else()
        message(WARNING
            "Inno Setup's ISCC was not found. Everything builds, but the package target cannot "
            "produce an installer until it is installed or IKONIKA_ISCC is set.")
    endif()

    include(CPack)
endif()
