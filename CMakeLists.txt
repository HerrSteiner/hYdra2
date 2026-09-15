cmake_minimum_required(VERSION 3.21)

project(hYdra2
    VERSION 0.6.0
    DESCRIPTION "Modern editor for Csound HETRO/adsyn analysis files"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HYDRA2_MIN_QT_VERSION 6.9)

find_package(Qt6 ${HYDRA2_MIN_QT_VERSION} REQUIRED COMPONENTS Core Qml Quick QuickControls2 Graphs)
qt_standard_project_setup(REQUIRES ${HYDRA2_MIN_QT_VERSION})

add_library(hydra2_core STATIC
    src/HetFile.cpp
    src/HetFile.hpp
    src/HetTypes.hpp
    src/HydraDocument.cpp
    src/HydraDocument.hpp
)

target_include_directories(hydra2_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(hydra2_core PUBLIC Qt6::Core)

qt_add_executable(hydra2
    MACOSX_BUNDLE
    src/main.cpp
    src/BreakpointGraphModel.cpp
    src/BreakpointGraphModel.hpp
    src/PartialMixer.cpp
    src/PartialMixer.hpp
)

target_compile_definitions(hydra2
    PRIVATE
        HYDRA2_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(hydra2
    PRIVATE
        hydra2_core
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Graphs
)

qt_add_qml_module(hydra2
    URI Hydra2
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

set_target_properties(hydra2 PROPERTIES
    OUTPUT_NAME "hYdra2"
    MACOSX_BUNDLE_BUNDLE_NAME "hYdra2"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.mobileart.hydra2"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS hydra2
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET hydra2
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 ${HYDRA2_MIN_QT_VERSION} REQUIRED COMPONENTS Test)
    qt_add_executable(tst_hetfile
        tests/tst_hetfile.cpp
    )
    target_link_libraries(tst_hetfile PRIVATE hydra2_core Qt6::Test)
    add_test(NAME hetfile COMMAND tst_hetfile)
endif()
