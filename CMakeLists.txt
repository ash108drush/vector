cmake_minimum_required(VERSION 3.16)

project(untitled41 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(untitled41 main.cpp
    vector.h)

include(GNUInstallDirs)
install(TARGETS untitled41
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
