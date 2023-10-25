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