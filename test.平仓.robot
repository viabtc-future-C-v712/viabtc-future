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
限价平多(开发测试1)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000     30000
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    5000    30000

    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    50000     35000
    put open     ${Carol}    ${空}    ${限价}    ${逐仓}    5000     35000
    put close     ${Alice}    ${多}    ${限价}    ${逐仓}    5000    35000
市价平多(未成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    5000
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000
    check order    ${Alice}  # 市价不挂单
    check order book
    check balance    ${Alice}    BCH    ${可用余额}    799950  # 开多5000,成交5000 (-50), 平多5000未成交(0) 余额 是800000 - 50
    check position    ${Alice}    ${多}    ${可用仓位}    5000  # 开多成交了5000 (+5000), 平多未成交不会冻结了 结果是 5000
    check position    ${Alice}    ${多}    ${冻结仓位}    0
市价平多(部份成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个
    check order    ${Alice}  # 市价不挂单
    # -50， +20 -5 (800000 -35)
    check balance    ${Alice}    BCH    ${可用余额}    799965
    check position    ${Alice}    ${多}    ${可用仓位}    2500
    check position    ${Alice}    ${多}    ${冻结仓位}    0
限价平多(部份成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500  #新挂买单
    put close     ${Alice}    ${多}    ${限价}    ${逐仓}    5000  #市价平多
    check order    ${Alice}    amount=2500  # 市价不挂单
    # -50
    check balance    ${Alice}    BCH    ${可用余额}    799965
    # 成交了2500单， 剩下的2500为冻结
    check position    ${Alice}    ${多}    ${可用仓位}    0
    check position    ${Alice}    ${多}    ${冻结仓位}    2500
市价平多(部份成交,亏损)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    7999  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个
    check order    ${Alice}  # 市价不挂单
    # -50， +20 -5 (800000 -35)
    check balance    ${Alice}    BCH    ${可用余额}    799964.6875
    check position    ${Alice}    ${多}    ${可用仓位}    2500
    check position    ${Alice}    ${多}    ${冻结仓位}    0
市价平多(部份成交,盈利)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    8001  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个
    check order    ${Alice}  # 市价不挂单
    # -50， +20 -5 (800000 -35)
    check balance    ${Alice}    BCH    ${可用余额}    799965.3125
    check position    ${Alice}    ${多}    ${可用仓位}    2500
    check position    ${Alice}    ${多}    ${冻结仓位}    0
调整保证金(增加)
    init balance all    asset=USDT
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    8001  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个

    put open     ${Alice}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Bob}    ${多}    ${市价}    ${逐仓}    10000  # 成交5000
    put open     ${Alice}    ${多}    ${限价}    ${逐仓}    2500    8001  #新挂买单
    put close     ${Bob}    ${多}    ${市价}    ${逐仓}    5000  #市价平多 2500个

    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000    market=BTCUSDT
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000    market=BTCUSDT  # 成交5000
    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500    8001    market=BTCUSDT  #新挂买单
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000    market=BTCUSDT  #市价平多 2500个

    check order    ${Alice}  # 市价不挂单
    check balance    ${Alice}    BCH    ${可用余额}    799890.3125
    check position    ${Alice}    ${多}    ${可用仓位}    5000
    check position    ${Alice}    ${多}    ${冻结仓位}    0
    check position    ${Alice}    ${多}    ${保证金}    62.4875
    position update    ${Alice}    ${多}    10
    check balance    ${Alice}    BCH    ${可用余额}    799880.3125
    check position    ${Alice}    ${多}    ${保证金}    72.4875
    position update    ${Alice}    ${多}    -10
    check balance    ${Alice}    BCH    ${可用余额}    799890.3125
    check position    ${Alice}    ${多}    ${保证金}    62.4875
    check position all    ${Alice}    amount=5000