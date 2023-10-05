project(matchengine)

include_directories(${PROJECT_SOURCE_DIR}/inc)

file(GLOB sources "./src/*.c")
add_executable(matchengine.exe ${sources})
target_link_libraries(matchengine.exe network utils jansson ev mpdec mysqlclient rdkafka m pthread dl hiredis curl ssl crypto z lz4 -Wl,-Bdynamic)

set_target_properties(matchengine.exe
	PROPERTIES
	INSTALL_RPATH "$ORIGIN"
	CMAKE_INSTALL_RPATH "$ORIGIN"
	LINK_FLAGS "-Wl,--disable-new-dtags -Wl,-rpath,'/usr/local/lib:$ORIGIN/../lib:$ORIGIN/./'"
)

install(TARGETS matchengine.exe DESTINATION /viabtc-deal/bin/)