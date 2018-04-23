cmake_minimum_required(VERSION 2.8)

project(AndroidAccessory)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package ( USB1 REQUIRED )

include_directories ( ${LIBUSB_INCLUDE_DIR} )

add_library(androidaccessory
	STATIC
	androidaccessory.cpp
)

target_link_libraries(androidaccessory
	${LIBUSB_LIBRARIES}
)

add_executable(TestDemo
	test.cpp
)

target_link_libraries(TestDemo
	androidaccessory
)
