cmake_minimum_required(VERSION 3.20)

project(brainfxxck LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_path(LibEdit_INCLUDE_DIRS histedit.h PATHS /usr/include)
find_library(LibEdit_LIBRARIES edit PATHS /usr/lib/x86_64-linux-gnu)

find_package(LLVM REQUIRED CONFIG)
if(LLVM_VERSION_MAJOR VERSION_LESS 18)
    message(FATAL_ERROR "LLVM version ${LLVM_VERSION} is too old. Need at least 18.x")
endif()

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

if(LLVM_LINK_LLVM_DYLIB)
    set(llvm_libs LLVM)
else()
    llvm_map_components_to_libnames(llvm_libs
        core executionengine orcjit passes nativecodegen support target mc
        AllTargetsAsmParsers AllTargetsCodeGens AllTargetsDescs AllTargetsInfos
    )
endif()

find_package(pegtl QUIET)
if(NOT pegtl_FOUND)
    include(FetchContent)
    FetchContent_Declare(pegtl
        GIT_REPOSITORY https://github.com/taocpp/PEGTL.git
        GIT_TAG 3.2.7
    )
    FetchContent_MakeAvailable(pegtl)
endif()

add_library(brainfxxck_lib STATIC
    src/ast.cpp
    src/parser.cpp
    src/codegen.cpp
    src/jit.cpp
    src/compiler.cpp
)

target_include_directories(brainfxxck_lib PUBLIC include)
target_link_libraries(brainfxxck_lib PUBLIC ${llvm_libs})
if(TARGET taocpp::pegtl)
    target_link_libraries(brainfxxck_lib PUBLIC taocpp::pegtl)
endif()

add_executable(brainfxxck src/main.cpp)
target_link_libraries(brainfxxck PRIVATE brainfxxck_lib)

install(TARGETS brainfxxck
    RUNTIME DESTINATION bin
)

install(TARGETS brainfxxck_lib
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
