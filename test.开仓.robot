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

# Test Setup   init balance all
# Test Teardown   重启
*** Variables ***

*** Test Cases ***
市价开多(未成交)
    ${max_offset1}=    Evaluate    test.get_max_offset('orders')
    市价开多(未成交)
    ${max_offset2}=    Evaluate    test.get_max_offset('orders')
    Should Be Equal As Numbers    ${max_offset1+1}    ${max_offset2}    # 发了ORDER_EVENT_FINISH
    check order    ${Alice}
市价开空(未成交)
    put open     ${Alice}    ${空}    ${市价}    ${逐仓}    10000
    check order    ${Alice}
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