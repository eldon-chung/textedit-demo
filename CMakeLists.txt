cmake_minimum_required(VERSION 3.20)
project(textedit-demo CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.16.0)
FetchContent_MakeAvailable(httplib)

add_executable(textedit
    main.cpp
    src/buffer/ArrayBuffer.cpp
    src/buffer/BufferVisitor.cpp
    src/buffer/PieceTable.cpp
    src/buffer/RopeBuffer.cpp
    src/editor/Editor.cpp
    src/display/TUIDisplay.cpp
    src/server/VisServer.cpp)

target_include_directories(textedit PRIVATE
    src
    ${CURSES_INCLUDE_DIR}
    ${httplib_SOURCE_DIR})

target_link_libraries(textedit PRIVATE
    ${CURSES_LIBRARIES}
    httplib::httplib
    Threads::Threads)

target_compile_definitions(textedit PRIVATE CPPHTTPLIB_THREAD_POOL_SIZE=2)
