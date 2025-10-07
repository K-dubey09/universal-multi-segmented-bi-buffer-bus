cmake_minimum_required(VERSION 3.10)
project(Somakernel C)

include_directories(include)
file(GLOB SOURCES "src/*.c")

# Allow building either a static or shared library via -DBUILD_SHARED_LIBS=ON
add_library(somakernel ${SOURCES})

# On MSVC, export all symbols for the shared library so consumers (like ctypes)
# can load functions without requiring per-symbol __declspec(dllexport).
if(MSVC)
	set_target_properties(somakernel PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

add_executable(test_somakernel test/test_somakernel.c)
target_link_libraries(test_somakernel somakernel)