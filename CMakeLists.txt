cmake_minimum_required(VERSION 3.15)
project(MyCrowProject)

# Include vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)



# Define the executable
add_executable(MyCrowProject src/main.cpp src/user.cpp)

# Find packages
find_package(asio REQUIRED)
find_package(Crow CONFIG REQUIRED)

# Link Crow and other libraries
target_link_libraries(MyCrowProject PRIVATE Crow::Crow)
if (WIN32)
    target_link_libraries(MyCrowProject PRIVATE ws2_32 mswsock)
endif()
target_link_libraries(MyCrowProject PRIVATE odbc32)

# Specify the directory where the Argon2 static library is located
link_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2")

# Include Argon2 header files
include_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2/include")

# Link Argon2 static library
target_link_libraries(MyCrowProject PRIVATE "${CMAKE_SOURCE_DIR}/phc-winner-argon2/libargon2.a")