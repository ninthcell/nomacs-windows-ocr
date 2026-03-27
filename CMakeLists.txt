cmake_minimum_required(VERSION 3.16)
project(OcrPlugin)

# --- nomacs plugin build utilities ---
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Utils.cmake")

NMC_POLICY()
NMC_PREPARE_PLUGIN()
NMC_FINDQT()
NMC_FIND_OPENCV("core" "imgproc")

include_directories(
    ${QT_INCLUDES}
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${NOMACS_INCLUDE_DIRECTORY}
    ${NOMACS_INCLUDE_DIRECTORY}/../DkGui
    ${NOMACS_INCLUDE_DIRECTORY}/..
)

file(GLOB PLUGIN_SOURCES "src/*.cpp")
file(GLOB PLUGIN_HEADERS "src/*.h" "${NOMACS_INCLUDE_DIRECTORY}/DkPluginInterface.h")
file(GLOB PLUGIN_JSON "src/*.json")

set_source_files_properties("${NOMACS_INCLUDE_DIRECTORY}/DkPluginInterface.h" PROPERTIES SKIP_AUTOMOC ON)

NMC_PLUGIN_ID_AND_VERSION()

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)
add_definitions(-DQT_DLL)

link_directories(
    ${OpenCV_LIBRARY_DIRS}
    ${NOMACS_BUILD_DIRECTORY}/libs
    ${NOMACS_BUILD_DIRECTORY}
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

target_link_libraries(
    ${PROJECT_NAME}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTGUI_LIBRARY}
    ${QT_QTNETWORK_LIBRARY}
    ${QT_QTMAIN_LIBRARY}
    ${OpenCV_LIBS}
    ${NOMACS_LIBS}
)
target_link_libraries(${PROJECT_NAME} Qt::Widgets Qt::Gui Qt::Concurrent)

# Windows OCR — C++/WinRT + WindowsApp.lib
if(WIN32)
    target_link_libraries(${PROJECT_NAME} WindowsApp.lib)
endif()

NMC_CREATE_TARGETS()
NMC_GENERATE_USER_FILE()
NMC_GENERATE_PACKAGE_XML(${PLUGIN_JSON})
