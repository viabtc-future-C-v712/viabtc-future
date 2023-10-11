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
市价开空(吃多个多单)
    生成 order book
    ${deals_offset1}=    Evaluate    test.get_max_offset('deals')
    put open     ${Carol}    ${空}    ${市价}    ${逐仓}    15000    价格=7990  #可以吃掉两个order
    kafka deals    ${Carol}    ${Alice}    10000    ${deals_offset1 + 1}  #发了一条deal信息
    kafka deals    ${Carol}    ${Bob}    5000    ${deals_offset1 + 2}  #发了一条deal信息
    kafka deals empty    ${deals_offset1 + 3}
市价开多(吃多个空单)
    生成 order book
    ${deals_offset1}=    Evaluate    test.get_max_offset('deals')
    put open     ${Carol}    ${多}    ${市价}    ${逐仓}    15000    价格=8010  #可以吃掉两个order
    kafka deals    ${Alice}    ${Carol}    10000    ${deals_offset1 + 1}  #发了一条deal信息
    kafka deals    ${Bob}    ${Carol}    5000    ${deals_offset1 + 2}  #发了一条deal信息
    kafka deals empty    ${deals_offset1 + 3}
限价开空(吃多个多单)
    生成 order book
    ${deals_offset1}=    Evaluate    test.get_max_offset('deals')
    put open     ${Carol}    ${空}    ${限价}    ${逐仓}    150000    价格=7998  #可以吃掉两个order
    kafka deals    ${Carol}    ${Alice}    10000    ${deals_offset1 + 1}  #发了一条deal信息
    kafka deals    ${Carol}    ${Bob}    10000    ${deals_offset1 + 2}  #发了一条deal信息
    kafka deals empty    ${deals_offset1 + 3}
限价开多(吃多个空单)
    生成 order book
    ${deals_offset1}=    Evaluate    test.get_max_offset('deals')
    put open     ${Carol}    ${多}    ${限价}    ${逐仓}    150000    价格=8002  #可以吃掉两个order
    kafka deals    ${Alice}    ${Carol}    10000    ${deals_offset1 + 1}  #发了一条deal信息
    kafka deals    ${Bob}    ${Carol}    10000    ${deals_offset1 + 2}  #发了一条deal信息
    kafka deals empty    ${deals_offset1 + 3}
    check order depth
*** Keywords ***
生成 order book
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=8009
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=80010
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=8007
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=8008
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=8005
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=8006
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=8003
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=8004
    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    10000    价格=8001
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000    价格=8002

    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=7999
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=7998
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=7997
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=7996
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=7995
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=7994
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=7993
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=7992
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    10000    价格=7991
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    10000    价格=7990
    check order book    side=${空}    limit=10
    check order book    side=${多}    limit=10