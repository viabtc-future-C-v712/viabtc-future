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
帐户操作
    balance update    ${Alice}    BCH    800000
    重启服务
    sleep    3    # 等待服务启完毕
    check balance    ${Alice}    BCH    ${可用余额}    1600000

下单操作
    生成 order book
    重启服务
    sleep    3    # 等待服务启完毕
    check order book    side=${空}    limit=10
    check order book    side=${多}    limit=10

*** Keywords ***
生成 order book
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=12010
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=12019
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=11008
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=11007
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=10006
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=10005
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=9004
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=9003
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=8002
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=8001

    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=7999
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=7998
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=6997
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=6996
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=5995
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=5994
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=4993
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=4992
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=3991
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=3990
    check order book    side=${空}    limit=10
    check order book    side=${多}    limit=10