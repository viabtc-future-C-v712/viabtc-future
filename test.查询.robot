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

Test Setup   init balance all
Test Teardown   重启服务 并清理数据库
*** Variables ***

*** Test Cases ***
position mode query
    check position mode    ${Alice}    BTCBCH
position mode adjust
    position mode adjust    ${Alice}    BTCBCH
uscase case 2
    check order book    market=BTCBCH    side=1    offset=0    limit=100
市价开多(未成交)
    市价开多(未成交)
    check order    ${Alice}
仓位查询
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    8001  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个
    check order    ${Alice}  # 市价不挂单
    # -50， +20 -5 (800000 -35)
    check balance    ${Alice}    BCH    ${可用余额}    799965.3125
    check position    ${Alice}    market=BTCBCH    side=${多}    type=${可用仓位}    amount=2500
    check position    ${Alice}    market=BTCBCH    side=${多}    type=${冻结仓位}    amount=0