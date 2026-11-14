cmake_minimum_required(VERSION 4.0)
include(FetchContent)

project(MultimediaRetrieval)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "/home/dafitius/Documents/bin/qt/6.11.2/gcc_64/lib/cmake/;C:/Qt/6.11.2/msvc2022_64/lib/cmake/;C:/Qt/6.11.2/mingw_64/lib/cmake/")

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

message("Building with profile ${CMAKE_BUILD_TYPE}")

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
        GIT_TAG 	    8d1fd52e5ab5590e2c81768ace50c72bae28f2ed #refs/tags/1.0.3
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
        qhull
        GIT_REPOSITORY  https://github.com/qhull/qhull
        GIT_TAG         d1c2fc0caa5f644f3a0f220290d4a868c68ed4f6 #refs/tags/v8.1-alpha6
)
FetchContent_Declare(
        nanoflann
        GIT_REPOSITORY  https://github.com/jlblancoc/nanoflann.git
        GIT_TAG         9a653cb243db6a09c94f833b28732b62f033e2b5 #refs/tags/v1.7.1
        SOURCE_SUBDIR   header_only
)
FetchContent_Declare(
        Eigen3
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3.4.0
        GIT_SHALLOW    TRUE
)

set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Eigen3)

FetchContent_MakeAvailable(nanoflann)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)
FetchContent_MakeAvailable(qhull)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(FetchContent)
FetchContent_Declare(meshfix GIT_REPOSITORY https://github.com/alex-mextner/meshfix.git GIT_TAG v0.1.1)
set(MESHFIX_BUILD_CLI OFF)
set(MESHFIX_BUILD_TESTS OFF)
FetchContent_MakeAvailable(meshfix)

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

set(VCPKG_APPLOCAL_DEPS OFF)

qt_add_executable(${PROJECT_NAME} ${main_src} resources/resources.qrc)

target_include_directories(${PROJECT_NAME} PRIVATE "${qhull_SOURCE_DIR}/src" "${nanoflann_SOURCE_DIR}/include")

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Quick
        Qt6::QuickWidgets
        glm::glm
        qhullcpp
        qhullstatic_r
        Eigen3::Eigen
)

file(GLOB_RECURSE database_generator_src
        "${CMAKE_CURRENT_SOURCE_DIR}/src/geometry/*.cpp"
)

add_executable(generate_database
        tools/generate_database.cpp
        ${database_generator_src}
        src/io/obj_file.cpp
        src/querying/feature_vector.cpp
        src/stats/distributions.cpp
)

target_include_directories(generate_database PRIVATE "${qhull_SOURCE_DIR}/src")

target_link_libraries(generate_database PRIVATE
        Qt6::Core
        Qt6::Gui
        glm::glm
        qhullcpp qhullstatic_r meshfix::meshfix_static
)

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    # Only look next to the Qt we link against, so a windeployqt from another Qt install (vcpkg) is never used
    unset(WINDEPLOYQT_EXECUTABLE CACHE)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt PATHS "${QT_BIN_DIR}" NO_DEFAULT_PATH NO_CACHE)

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
        CACHED_DB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/CachedShapeDatabase"
        TEST_SHAPE_DB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/TestShapeDatabase"
)

target_compile_definitions(generate_database PRIVATE
        SHAPE_DB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/ShapeDatabase_INFOMR"
        CACHED_DB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/CachedShapeDatabase"
)
