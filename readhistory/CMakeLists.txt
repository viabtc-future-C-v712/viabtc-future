project(readhistory)

include_directories(${PROJECT_SOURCE_DIR}/inc)

file(GLOB sources "./src/*.c")
add_executable(readhistory.exe ${sources})
target_link_libraries(readhistory.exe network utils jansson ev mpdec mysqlclient rdkafka m pthread dl hiredis curl ssl crypto z lz4 -Wl,-Bdynamic)

set_target_properties(readhistory.exe
	PROPERTIES
	INSTALL_RPATH "$ORIGIN"
	CMAKE_INSTALL_RPATH "$ORIGIN"
	LINK_FLAGS "-Wl,--disable-new-dtags -Wl,-rpath,'/usr/local/lib:$ORIGIN/../lib:$ORIGIN/./'"
)

install(TARGETS readhistory.exe DESTINATION /viabtc-deal/bin/)