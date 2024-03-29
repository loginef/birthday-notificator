cmake_minimum_required(VERSION 3.12)
project(telegram_bot CXX)

include(third_party/userver/cmake/SetupEnvironment.cmake)
include(GNUInstallDirs)

add_subdirectory(third_party/userver)
add_subdirectory(third_party/tgbot-cpp)
add_subdirectory(src/messages)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

# Common sources
add_library(${PROJECT_NAME}_objs OBJECT
    src/components/birthday_notificator.hpp
    src/components/birthday_notificator.cpp
    src/components/bot/impl/component.hpp
    src/components/bot/impl/component.cpp
    src/components/bot/impl/http_client.hpp
    src/components/bot/impl/http_client.cpp
    src/components/bot/impl/reply_markup.hpp
    src/components/bot/impl/reply_markup.cpp
    src/components/bot/component.hpp
    src/components/bot/component.cpp
    src/models/birthday.hpp
    src/models/birthday.cpp
    src/models/button.hpp
    src/models/button.cpp
    src/models/time_point.hpp
    src/models/user.hpp
    src/db/birthdays.hpp
    src/db/birthdays.cpp
    src/db/users.hpp
    src/db/users.cpp
    ${PROTO_HDRS}
    ${PROTO_SRCS}
)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/messages)
target_include_directories(${PROJECT_NAME}_objs PUBLIC src)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver-postgresql TgBot messages)


# The Service
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)


# Unit Tests
add_executable(${PROJECT_NAME}_unittest
    src/components/birthday_notificator_test.cpp
)
target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver-utest)
add_google_tests(${PROJECT_NAME}_unittest)


# Benchmarks
# add_executable(${PROJECT_NAME}_benchmark
# 	src/hello_benchmark.cpp
# )
# target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver-ubench)
# add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

# Functional Tests
include(UserverTestsuite)

userver_testsuite_add_simple(
    REQUIREMENTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/requirements.txt"
    PYTHONPATH "${CMAKE_CURRENT_BINARY_DIR}/src/messages"
)

if(DEFINED ENV{PREFIX})
	message(STATUS "Set install prefix: $ENV{PREFIX}")
	file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
	set(CMAKE_INSTALL_PREFIX ${PREFIX_PATH})
endif()

file(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${PROJECT_NAME})
install(FILES ${CONFIGS_FILES} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME} COMPONENT ${PROJECT_NAME})

