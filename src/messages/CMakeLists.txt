find_package(Protobuf)
if(NOT Protobuf_FOUND)
message(FATAL_ERROR
    "failed to find Protobuf compiler.\n"
    "Please install the packages required for your system:\n\n"
    "  Debian:    sudo apt install protobuf-compiler python3-protobuf\n"
    "  macOS:     brew install protobuf\n"
    "  ArchLinux: sudo pacman -S protobuf\n"
    "  FreeBSD:   pkg install protobuf\n")
endif()

message(${CMAKE_CURRENT_BINARY_DIR})

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS button.proto)
add_library(messages STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(messages ${Protobuf_LIBRARIES})

protobuf_generate_python(PROTO_PY_FILES button.proto)
file(COPY python/__init__.py DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
add_custom_command(TARGET messages POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${PROTO_PY_FILES} ${CMAKE_CURRENT_BINARY_DIR}/
    COMMENT "Copying protobuf python files to: ${CMAKE_CURRENT_BINARY_DIR}/"
)