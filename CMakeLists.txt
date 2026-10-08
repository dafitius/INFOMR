cmake_minimum_required(VERSION 4.0)
include(FetchContent)

project(MultimediaRetrieval)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "/home/dafitius/Documents/bin/qt/6.11.2/gcc_64/lib/cmake/;C:/Qt/6.11.2/msvc2022_64/lib/cmake/;C:/Qt/6.11.2/mingw_64/lib/cmake/")

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        OpenGL
        OpenGLWidgets
        Quick
        QuickWidgets
)

FetchContent_Declare(
        glm
        GIT_REPOSITORY	https://github.com/g-truc/glm.git
        GIT_TAG 	8d1fd52e5ab5590e2c81768ace50c72bae28f2ed #refs/tags/1.0.3
)

FetchContent_MakeAvailable(glm)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_source_files_properties(*.c LANGUAGE C)
set_source_files_properties(*.cc LANGUAGE CXX)
set_source_files_properties(*.cpp LANGUAGE CXX)
set_source_files_properties(*.h LANGUAGE C)
set_source_files_properties(*.hh LANGUAGE CXX)
set_source_files_properties(*.hpp LANGUAGE CXX)
set_source_files_properties(*.inl LANGUAGE CXX)

file(GLOB_RECURSE main_src
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_SOURCE_DIR}/src/*/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*/*.hpp"
        "${CMAKE_SOURCE_DIR}/src/*.tpp"
        "${CMAKE_SOURCE_DIR}/src/*/*.tpp"
)

source_group(${PROJECT_NAME} ${main_src})

qt_add_executable(${PROJECT_NAME} ${main_src} resources/resources.qrc)

target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/extern/glm")

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Quick
        Qt6::QuickWidgets
        glm::glm
)

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    $<IF:$<CONFIG:Debug>,--debug,--release>
                    --no-compiler-runtime
                    --no-translations
                    "$<TARGET_FILE:${PROJECT_NAME}>"
                COMMENT "Deploying Qt runtime with windeployqt")
    else()
        message(WARNING "windeployqt not found, the executable will need Qt's bin directory on PATH")
    endif()
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        SHAPE_DB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/ShapeDatabase_INFOMR"
)
