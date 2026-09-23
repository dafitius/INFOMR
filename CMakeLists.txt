cmake_minimum_required(VERSION 4.0)
project(MultimediaRetrieval)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "/home/dafitius/Documents/bin/qt/6.11.2/gcc_64/lib/cmake/; C:/Qt/6.11.2/mingw_64/lib/cmake/")

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        OpenGL
        OpenGLWidgets
        Quick
        QuickWidgets
        Quick3D
)

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

# Remove workshop sources if Quick3D unavailable (so target doesn't reference missing implementations)
qt_add_executable(${PROJECT_NAME} ${main_src} resources/resources.qrc)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Quick
                Qt6::QuickWidgets
        Qt6::Quick3D
)
