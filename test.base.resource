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

*** Variables ***

*** Keywords ***
市价开多(未成交)
    init balance all
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000