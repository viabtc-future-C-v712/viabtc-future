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

Test Setup   init balance all    asset=USDT
Test Teardown   重启服务 并清理数据库
*** Variables ***

*** Test Cases ***
逐仓
    生成 order book
    put open     ${Carol}    ${多}    ${市价}    ${逐仓}    10000
    put open     ${Alice}    ${空}    ${市价}    ${逐仓}    30000    价格=8001  #成交后的市场价格为8001 
    make slice
全仓
    生成 order book    market=BTCUSDT
    put open     ${Carol}    ${多}    ${市价}    ${全仓}    10000    market=BTCUSDT
    put open     ${Alice}    ${空}    ${市价}    ${逐仓}    30000    价格=8001    market=BTCUSDT  #成交后的市场价格为8001

    生成 order book    market=ETHUSDT
    put open     ${Carol}    ${多}    ${市价}    ${全仓}    10000    market=ETHUSDT
    put open     ${Alice}    ${空}    ${市价}    ${逐仓}    30000    价格=8001    market=ETHUSDT  #成交后的市场价格为8001

    make slice
*** Keywords ***
生成 order book
    [Arguments]  ${market}=BTCBCH
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=12010    market=${market}
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=12019    market=${market}
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=11008    market=${market}
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=11007    market=${market}
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=10006    market=${market}
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=10005    market=${market}
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=9004    market=${market}
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=9003    market=${market}
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=8002    market=${market}
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=8001    market=${market}

    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=7999    market=${market}
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=7998    market=${market}
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=6997    market=${market}
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=6996    market=${market}
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=5995    market=${market}
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=5994    market=${market}
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=4993    market=${market}
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=4992    market=${market}
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=3991    market=${market}
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=3990    market=${market}
    check order book    side=${空}    limit=10
    check order book    side=${多}    limit=10