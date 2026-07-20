cmake_minimum_required(VERSION 3.16)
project(surveillance VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GPIOD REQUIRED libgpiod)

add_executable(surveillance
    main.cpp
    logger.cpp
    gpio.cpp
    camera.cpp
    motion.cpp
    recorder.cpp
    stream.cpp
    alert.cpp
)

target_include_directories(surveillance PRIVATE ${OpenCV_INCLUDE_DIRS} ${GPIOD_INCLUDE_DIRS})
target_compile_options(surveillance PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(surveillance PRIVATE ${OpenCV_LIBS} ${GPIOD_LIBRARIES} Threads::Threads)

install(TARGETS surveillance RUNTIME DESTINATION bin)