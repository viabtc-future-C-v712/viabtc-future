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

# Test Setup   init balance all
# Test Teardown   重启
*** Variables ***

*** Test Cases ***
aaaa
    check order all    51324534    20000    0    10
充值
    call    'balance.update'    1, "USDT", "deposit", 16983029630, "1282031", {"detail": "detail"}
保存数据库
    make slice
重启服务 并清理数据库
    sleep    0.2s
    init oper Log
    call    'matchengine.suicide'    -1
order
    put limit    1, "BTCBCH", 2, "1", "8000", "0.002", "0.001", ""
    put limit    2, "BTCBCH", 1, "1", "7999", "0.002", "0.001", ""
kline
    redis    k:BTCBCH:last
*** Keywords ***
redis
    [Arguments]  ${key}
    ${redis_conn}=    Connect To Redis    127.0.0.1    6379
    ${data}=    Get From Redis    ${redis_conn}    ${key}
    Should Be Equal As Strings    ${data}    8000.00000000