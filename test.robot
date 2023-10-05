*** Settings ***
Library    RedisLibrary
Library    ConfluentKafkaLibrary
Library    OperatingSystem
Resource   test.db.resource
Resource   test.http.resource

Test Setup   init balance all
Test Teardown   重启
*** Variables ***
${BOOTSTRAP_SERVERS}    192.168.1.3:9092
${GROUP_ID}             test-group
${Alice}                1
${Bob}                  2
${可用余额}              1
${冻结余额}              2
${空}                   1
${多}                   2
${市价}                 0
${限价}                 1
${逐仓}                 1
${全仓}                 2
${close}                2
${open}                 1
${可用仓位}              position
${冻结仓位}              frozen
*** Test Cases ***
市价开多(未成交)
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000

    check order    ${Alice}    ${open}    ${多}    10000
市价开空(未成交)
    put open     ${Alice}    ${空}    ${市价}    ${逐仓}    10000

    check order    ${Alice}    ${open}    ${空}    10000
限价开空(未成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    10000

    check order    ${Bob}    ${open}    ${空}    10000
    check balance    ${Bob}    BCH    ${可用余额}    799900
    check position    ${Bob}    ${空}    ${可用仓位}    0
市价开多(部份成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000

    check order    ${Alice}    ${open}    ${多}    5000
    check balance    ${Alice}    BCH    ${可用余额}    799900
    check position    ${Alice}    ${多}    ${可用仓位}    5000
市价平多(未成交)
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    5000
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000
    # 成交了5000 所以还剩下 10000 - 5000
    check order    ${Alice}    ${open}    ${空}    5000
    # 开多5000,成交5000 (-50), 平多5000未成交(0) 余额 是800000 - 50
    check balance    ${Alice}    BCH    ${可用余额}    799950
    # 开多成交了5000 (+5000), 平多未成交冻结了(-5000) 结果是 0
    check position    ${Alice}    ${多}    ${可用仓位}    0
    check position    ${Alice}    ${多}    ${冻结仓位}    5000
市价平多(部份成交)
    init balance    ${Bob}    BCH
    balance update    ${Bob}    BCH    800000
    put open     ${Bob}    ${空}    ${限价}    ${逐仓}    5000

    init balance    ${Alice}    BCH
    balance update    ${Alice}    BCH    800000
    put open     ${Alice}    ${多}    ${市价}    ${逐仓}    10000

    put open     ${Bob}    ${多}    ${限价}    ${逐仓}    2500
    put close     ${Alice}    ${多}    ${市价}    ${逐仓}    5000

    check order    ${Alice}    ${open}    ${空}    0
    check balance    ${Alice}    BCH    ${可用余额}    799950
    check position    ${Alice}    ${多}    ${可用仓位}    200
保存数据库
    make slice
重启1
    重启
order
    put limit    1, "BTCBCH", 2, "1", "8000", "0.002", "0.001", ""
    put limit    2, "BTCBCH", 1, "1", "7999", "0.002", "0.001", ""
kline
    redis    k:BTCBCH:last
kafkaorders
    kafka    orders
    kafka    balances
    kafka    deals
*** Keywords ***
init
    balance update    ${Alice}    BCH    800000
    balance update    ${Bob}    BCH    800000
kafka
    [Arguments]  ${TOPIC}
    ${group_id}=  Create Consumer  auto_offset_reset=earliest
    Subscribe Topic    ${group_id}    ${TOPIC}
    ${messages}=  Poll  group_id=${group_id}  max_records=3  decode_format=utf8
    Close Consumer    ${group_id}
redis
    [Arguments]  ${key}
    ${redis_conn}=    Connect To Redis    192.168.1.3    6379
    ${data}=    Get From Redis    ${redis_conn}    ${key}
    Should Be Equal As Strings    ${data}    8000.00000000