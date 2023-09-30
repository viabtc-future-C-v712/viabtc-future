project(accesshttp)

include_directories(${PROJECT_SOURCE_DIR}/inc)

file(GLOB sources "./src/*.c")
add_executable(accesshttp.exe ${sources})
target_link_libraries(accesshttp.exe network utils jansson ev mpdec m hiredis -Wl,-Bdynamic)

set_target_properties(accesshttp.exe
	PROPERTIES
	INSTALL_RPATH "$ORIGIN"
	CMAKE_INSTALL_RPATH "$ORIGIN"
	LINK_FLAGS "-Wl,--disable-new-dtags -Wl,-rpath,'/usr/local/lib:$ORIGIN/../lib:$ORIGIN/./'"
)