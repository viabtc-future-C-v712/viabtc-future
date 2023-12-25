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
# Test Teardown   重启服务 并清理数据库
*** Variables ***

*** Test Cases ***
计划委托市价
    生成 order book
    put open     ${Carol}    ${空}    ${委托}    ${逐仓}    15000    价格=0    触发价格=8000   #价格高于触发价后下单，
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    价格=8001  #成交后的市场价格为8001
    check order book    market=BTCUSDT    side=1    offset=0    limit=100
计划委托限价
    生成 order book
    put open     ${Carol}    ${空}    ${委托}    ${逐仓}    15000    价格=9000    触发价格=8000   #价格高于触发价后下单，
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    价格=8001  #成交后的市场价格为8001
    check order book    market=BTCUSDT    side=1    offset=0    limit=100
计划委托转市价
    生成 order book
    put open     ${Carol}    ${空}    ${委托}    ${逐仓}    15000    价格=0    触发价格=8000   #价格高于触发价后下单，
    check order    ${Carol}    amount=15000    type=${委托}
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    价格=8001  #成交后的市场价格为8001
    check order    ${Carol}    amount=0    type=${限价}
    check order book    market=BTCUSDT    side=1    offset=0    limit=100
计划委托转限价
    生成 order book
    put open     ${Carol}    ${空}    ${委托}    ${逐仓}    15000    价格=9000    触发价格=8000   #价格高于触发价后下单，
    check order    ${Carol}    amount=15000    type=${委托}
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    价格=8001  #成交后的市场价格为8001
    check order    ${Carol}    amount=15000    type=${限价}
    order cancel    ${Carol}    BTCBCH    23
    check order book    market=BTCUSDT    side=1    offset=0    limit=100
计划委托转市价 失败
    生成 order book
    put open     ${Carol}    ${空}    ${委托}    ${逐仓}    15000    价格=0    触发价格=8000   #价格高于触发价后下单，
    check order    ${Carol}    amount=15000    type=${委托}
    init balance    ${Carol}
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    价格=8001  #成交后的市场价格为8001
    check order    ${Carol}    amount=15000    type=${委托}
    check order book    market=BTCUSDT    side=1    offset=0    limit=100
计划委托转限价 失败
    生成 order book
    put open     ${Carol}    ${空}    ${委托}    ${逐仓}    15000    价格=9000    触发价格=8000   #价格高于触发价后下单，
    check order    ${Carol}    amount=15000    type=${委托}
    init balance    ${Carol}
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    价格=8001  #成交后的市场价格为8001
    check order    ${Carol}    amount=15000    type=${委托}
    check order book    market=BTCUSDT    side=1    offset=0    limit=100
计划委托(取消)
    生成 order book
    put open     ${Carol}    ${空}    ${委托}    ${逐仓}    15000    价格=8000    触发价格=8000   #价格高于触发价后下单，

    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    价格=8001  #成交后的市场价格为8001
止盈
    生成 order book
    put open     ${Carol}    ${多}    ${市价}    ${逐仓}    10000    价格=8001  #成交后的市场价格为8001
    put close     ${Alice}    ${空}    ${止盈止损}    ${逐仓}    0    价格=0    触发价格=8000   #价格高于触发价后下单，
    put open     ${Carol}    ${空}    ${市价}    ${逐仓}    10000    价格=9000   #价格高于触发价后下单，
    check order book    market=BTCUSDT    side=1    offset=0    limit=100
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