project(accessws)

include_directories(${PROJECT_SOURCE_DIR}/inc)

file(GLOB sources "./src/*.c")
add_executable(accessws.exe ${sources})
target_link_libraries(accessws.exe network utils jansson ev mpdec pthread curl rdkafka crypto ssl crypto ssl dl lz4 z m hiredis -Wl,-Bdynamic)

set_target_properties(accessws.exe
	PROPERTIES
	INSTALL_RPATH "$ORIGIN"
	CMAKE_INSTALL_RPATH "$ORIGIN"
	LINK_FLAGS "-Wl,--disable-new-dtags -Wl,-rpath,'/usr/local/lib:$ORIGIN/../lib:$ORIGIN/./'"
)

install(TARGETS accessws.exe DESTINATION /viabtc-deal/bin/)