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
帐户
    balance update    ${Alice}    BCH    800000
    重启服务
    sleep    3    # 等待服务启完毕
    check balance    ${Alice}    BCH    ${可用余额}    1600000