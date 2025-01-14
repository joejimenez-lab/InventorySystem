cmake_minimum_required(VERSION 3.15)
project(MyCrowProject)

# Include vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Define the executable
add_executable(MyCrowProject src/main.cpp)

# Find packages
find_package(asio REQUIRED)
find_package(Crow CONFIG REQUIRED)

# Link Crow
target_link_libraries(MyCrowProject PRIVATE Crow::Crow)