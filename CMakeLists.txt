cmake_minimum_required(VERSION 3.22)
project(nipovpn)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -ggdb")

# Set the preprocessor flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MMD -MP")

# https://cmake.org/cmake/help/latest/module/FindBoost.html
find_package(Boost REQUIRED COMPONENTS system regex)

# /usr/local/lib/cmake/yaml-cpp/yaml-cpp-config.cmake
# declared variables: YAML_CPP_INCLUDE_DIR & YAML_CPP_LIBRARY_DIR
find_package(yaml-cpp REQUIRED)

# https://cmake.org/cmake/help/v3.6/module/FindOpenSSL.html
find_package(OpenSSL REQUIRED)


add_subdirectory(core)
add_subdirectory(server)
add_subdirectory(agent)


#add_custom_target(deb
#        COMMAND ${CMAKE_COMMAND} -E echo "Creating Debian package..."
#        COMMAND dpkg-deb --build ${DEBIANDIR} ${DEBFILE}
#        COMMENT "Building Debian package"
#)

