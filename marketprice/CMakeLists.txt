project(marketprice)

include_directories(${PROJECT_SOURCE_DIR}/inc)

file(GLOB sources "./src/*.c")
add_executable(marketprice.exe ${sources})
target_link_libraries(marketprice.exe network utils jansson ev mpdec mysqlclient rdkafka m pthread dl hiredis curl ssl crypto z lz4 -Wl,-Bdynamic)

set_target_properties(marketprice.exe
	PROPERTIES
	INSTALL_RPATH "$ORIGIN"
	CMAKE_INSTALL_RPATH "$ORIGIN"
	LINK_FLAGS "-Wl,--disable-new-dtags -Wl,-rpath,'/usr/local/lib:$ORIGIN/../lib:$ORIGIN/./'"
)

install(TARGETS marketprice.exe DESTINATION /viabtc-deal/bin/)