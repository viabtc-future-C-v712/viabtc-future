*** Settings ***
Library    RedisLibrary
Library    ConfluentKafkaLibrary
Library    OperatingSystem
Library    test
Resource   test.db.resource
Resource   test.http.resource
Resource   test.ws.resource
Resource   test.kafka.resource
Variables  test_variable.py

Test Setup   init balance all
Test Teardown   重启
*** Variables ***

*** Test Cases ***
市价开多(未成交)
    ${max_offset1}=    Evaluate    test.get_max_offset('orders')
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000
    ${max_offset2}=    Evaluate    test.get_max_offset('orders')
    Should Be Equal As Numbers    ${max_offset1+1}    ${max_offset2}    # 发了ORDER_EVENT_FINISH
    check order    ${Alice}
市价开空(未成交)
    put open     ${Alice}    ${空}    ${市价}    ${逐仓}    10000
    check order    ${Alice}
限价开空(未成交)
    ${orders_offset1}=    Evaluate    test.get_max_offset('orders')
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000
    ${orders_offset2}=    Evaluate    test.get_max_offset('orders')
    Should Be Equal As Numbers    ${orders_offset1 + 1}    ${orders_offset2}   #说明这里发了一条order信息
    kafka orders    ${Bob}    10000    ${orders_offset1 + 1}
    check order brief    ${Bob}    10000
    # check order detail    amount=10000    orderid=${orderid}
    # order cancel    ${Bob}
    # check order detail    amount=10000
    # check order alluser    amount=10000
    check order book    side=${空}
    check balance    ${Bob}    BCH    ${可用余额}    799900
    check position    ${Bob}    ${空}    ${可用仓位}    0
限价开空(未成交)(撤销)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000
    ${orderid} =  check order brief    ${Bob}    amount=10000
    order cancel    ${Bob}    orderid=${orderid}
    check balance    ${Bob}    BCH    ${可用余额}    800000
    check position    ${Bob}    ${空}    ${可用仓位}    0
市价开多(部份成交)
    ${orders_offset1}=    Evaluate    test.get_max_offset('orders')
    ${deals_offset1}=    Evaluate    test.get_max_offset('deals')
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000
    kafka orders    ${Bob}    5000    ${orders_offset1 + 1}
    kafka orders    ${Alice}    10000    ${orders_offset1 + 2}
    kafka deals    ${Bob}    ${Alice}    5000    ${deals_offset1 + 1}  #发了一条deal信息
    check order    ${Alice}
    market last
    check balance    ${Alice}    BCH    ${可用余额}    799950
    check position    ${Alice}    ${多}    ${可用仓位}    5000
市价平多(未成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    5000
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000
    check order    ${Alice}  # 市价不挂单
    check order book
    check balance    ${Alice}    BCH    ${可用余额}    799950  # 开多5000,成交5000 (-50), 平多5000未成交(0) 余额 是800000 - 50
    check position    ${Alice}    ${多}    ${可用仓位}    5000  # 开多成交了5000 (+5000), 平多未成交不会冻结了 结果是 5000
    check position    ${Alice}    ${多}    ${冻结仓位}    0
市价平多(部份成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个
    check order    ${Alice}  # 市价不挂单
    # -50， +20 -5 (800000 -35)
    check balance    ${Alice}    BCH    ${可用余额}    799965
    check position    ${Alice}    ${多}    ${可用仓位}    2500
    check position    ${Alice}    ${多}    ${冻结仓位}    0
限价平多(部份成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500  #新挂买单
    put close     ${Alice}    ${多}    ${限价}    ${逐仓}    5000  #市价平多
    check order    ${Alice}    amount=2500  # 市价不挂单
    # -50
    check balance    ${Alice}    BCH    ${可用余额}    799965
    # 成交了2500单， 剩下的2500为冻结
    check position    ${Alice}    ${多}    ${可用仓位}    0
    check position    ${Alice}    ${多}    ${冻结仓位}    2500
市价平多(部份成交,亏损)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    7999  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个
    check order    ${Alice}  # 市价不挂单
    # -50， +20 -5 (800000 -35)
    check balance    ${Alice}    BCH    ${可用余额}    799964.6875
    check position    ${Alice}    ${多}    ${可用仓位}    2500
    check position    ${Alice}    ${多}    ${冻结仓位}    0
市价平多(部份成交,盈利)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    8001  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个
    check order    ${Alice}  # 市价不挂单
    # -50， +20 -5 (800000 -35)
    check balance    ${Alice}    BCH    ${可用余额}    799965.3125
    check position    ${Alice}    ${多}    ${可用仓位}    2500
    check position    ${Alice}    ${多}    ${冻结仓位}    0
调整保证金(增加)
    init balance all    asset=USDT
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    8001  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个

    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Bob}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    2500    8001  #新挂买单
    put close     ${Bob}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个

    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000    market=BTCUSDT
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    market=BTCUSDT  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    8001    market=BTCUSDT  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000    market=BTCUSDT  #市价平多 2500个

    check order    ${Alice}  # 市价不挂单
    check balance    ${Alice}    BCH    ${可用余额}    799890.3125
    check position    ${Alice}    ${多}    ${可用仓位}    5000
    check position    ${Alice}    ${多}    ${冻结仓位}    0
    check position    ${Alice}    ${多}    ${保证金}    62.4875
    position update    ${Alice}    ${多}    10
    check balance    ${Alice}    BCH    ${可用余额}    799880.3125
    check position    ${Alice}    ${多}    ${保证金}    72.4875
    position update    ${Alice}    ${多}    -10
    check balance    ${Alice}    BCH    ${可用余额}    799890.3125
    check position    ${Alice}    ${多}    ${保证金}    62.4875
    check position all    ${Alice}    amount=5000
仓位查询
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

test1
    position.query
保存数据库
    make slice
清理数据库
    init oper Log
重启1
    重启
order
    put limit    1, "BTCBCH", 2, "1", "8000", "0.002", "0.001", ""
    put limit    2, "BTCBCH", 1, "1", "7999", "0.002", "0.001", ""
kline
    redis    k:BTCBCH:last
*** Keywords ***
redis
    [Arguments]  ${key}
    ${redis_conn}=    Connect To Redis    192.168.1.3    6379
    ${data}=    Get From Redis    ${redis_conn}    ${key}
    Should Be Equal As Strings    ${data}    8000.00000000