cmake_minimum_required(VERSION 3.15)
project(MyCrowProject)

# Include vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Define the executable
add_executable(MyCrowProject src/main.cpp src/user.cpp)

# Find packages
find_package(asio REQUIRED)
find_package(Crow CONFIG REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/phc-winner-argon2/include)


# Link Crow
target_link_libraries(MyCrowProject PRIVATE Crow::Crow)
if (WIN32)
    target_link_libraries(MyCrowProject PRIVATE ws2_32 mswsock)
endif()
target_link_libraries(MyCrowProject PUBLIC odbc32)