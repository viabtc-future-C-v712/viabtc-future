*** Settings ***
Library    RedisLibrary
Library    ConfluentKafkaLibrary
Library    OperatingSystem
Library    test
Resource   test.db.resource
Resource   test.http.resource
Resource   test.ws.resource
Resource   test.kafka.resource
Resource   test.base.resource
Variables  test_variable.py

Test Setup   登陆
Test Teardown   退出
*** Variables ***

*** Test Cases ***
市价开多(未成交)
    市价开多(未成交)
    check order    ${Alice}
仓位定阅
    ${my_websocket} =  wscall start
    wscall send    ${my_websocket}    server.auth    "${Alice}",""
    sleep   1s
    wscall send    ${my_websocket}    position.subscribe    "BTCBCH",1,"BTCBCH",2
    wscall recv    ${my_websocket}
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    8001  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个
    check order    ${Alice}  # 市价不挂单
    # -50， +20 -5 (800000 -35)
    check balance    ${Alice}    BCH    ${可用余额}    799965.3125
    check position    ${Alice}    ${多}    ${可用仓位}    2500
    check position    ${Alice}    ${多}    ${冻结仓位}    0


    # wscall send    ${my_websocket}    position.query   "BTCBCH", ${多}
    wscall recv    ${my_websocket}
    sleep   1s
    wscall recv    ${my_websocket}
    sleep   1s
    wscall recv    ${my_websocket}
*** Keywords ***
登陆
    ${my_websocket} =  wscall start
    wscall send    ${my_websocket}    server.auth    "${Alice}",""
退出
    wscall end    ${my_websocket}
    # {"method": "server.auth","params": ["3",""],"id": 1}
    # {"method": "position.subscribe","params": ["BTCBCH",1,"BTCBCH",2],"id": 1}
    # {"method": "order.subscribe","params": ["BTCBCH"],"id": 1}