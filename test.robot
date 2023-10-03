*** Settings ***
Library    RequestsLibrary
Library    RedisLibrary
Library    ConfluentKafkaLibrary
Library    DatabaseLibrary
Library    OperatingSystem
Library    test
*** Variables ***
${file_path}            file.txt
${BASE_URL}             http://192.168.1.3:8080/
${BOOTSTRAP_SERVERS}    192.168.1.3:9092
${GROUP_ID}             test-group
${Alice}                1
${Bob}                  2
${可用余额}              1
${冻结余额}              2
${卖}                   1
${买}                   2
${市价}                 0
${限价}                 1
${逐仓}                 1
${全仓}                 2
${close}                2
${open}                 1
*** Test Cases ***
市价开多(未成交)
    make slice
    init balance    ${Alice}    BCH    ${冻结余额}
    init balance    ${Alice}    BCH    ${可用余额}
    balance update    ${Alice}    BCH    800000
    put open     ${Alice}    ${买}    ${市价}    ${逐仓}    10000
    make slice
    check order    ${Alice}    ${open}    ${买}    0
市价开空(未成交)
    make slice
    init balance    ${Alice}    BCH    ${冻结余额}
    init balance    ${Alice}    BCH    ${可用余额}
    balance update    ${Alice}    BCH    800000
    put open     ${Alice}    ${卖}    ${市价}    ${逐仓}    10000
    make slice
    check order    ${Alice}    ${open}    ${卖}    0
限价开空(未成交)
    make slice
    init balance    ${Bob}    BCH    ${冻结余额}
    init balance    ${Bob}    BCH    ${可用余额}
    balance update    ${Bob}    BCH    800000
    put open     ${Bob}    ${卖}    ${限价}    ${逐仓}    10000
    make slice
    check order    ${Bob}    ${open}    ${卖}    10000
市价开多(部份成交)
    make slice
    init balance    ${Bob}    BCH    ${冻结余额}
    init balance    ${Bob}    BCH    ${可用余额}
    balance update    ${Bob}    BCH    800000
    put open     ${Bob}    ${卖}    ${限价}    ${逐仓}    5000

    init balance    ${Alice}    BCH    ${冻结余额}
    init balance    ${Alice}    BCH    ${可用余额}
    balance update    ${Alice}    BCH    800000
    sleep    50ms
    put open     ${Alice}    ${买}    ${市价}    ${逐仓}    10000
    make slice
    check order    ${Alice}    ${open}    ${买}    0
order
    put limit    1, "BTCBCH", 2, "1", "8000", "0.002", "0.001", ""
    put limit    2, "BTCBCH", 1, "1", "7999", "0.002", "0.001", ""
kline
    redis    k:BTCBCH:last
kafkaorders
    kafka    orders
    kafka    balances
    kafka    deals
check balance 1
    check balance    1    BCH    1    800000
    check balance    2    BCH    1    800000
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
put limit
    [Arguments]  ${data}
    call    'order.put_limit'    ${data}
put open
    [Arguments]  ${用户}    ${买或卖}    ${市价或限价}     ${逐仓或全仓}    ${张}
    call    'order.open'    ${用户}, "BTCBCH", ${买或卖}, ${市价或限价}, ${逐仓或全仓}, "8000", "8000", "8000", "100", "${张}", "0.002", "0.001"
make slice
    call    'matchengine.makeslice'    ""
    sleep    1
call
    [Arguments]  ${method}    ${data}
    Create Session    user    ${BASE_URL}
    ${data2} =  Evaluate  {'method': ${method}, 'params': [${data}], 'id': 3}
    ${resp}=    POST On Session    user    /    json=${data2}
    Should Be Equal As Strings    ${resp.status_code}    200
    Should Be Equal As Strings    ${resp.json()["error"]}    None
init balance
    [Arguments]  ${id}    ${asset}    ${t}
    ${timestamps}  mysql    select time from slice_history order by id desc limit 1
    Log    ${timestamps[0][0]}
    ${balances}    mysql    select balance from slice_balance_${timestamps[0][0]} where user_id = ${id} and asset = '${asset}' and t = ${t}
    ${actual_size}    Get Length    ${balances}
    IF    '${actual_size}' != '0'
        ${actual_size}    Get Length    ${balances[0]}
        IF    '${actual_size}' != '0'
            ${balance}    Convert To Number    ${balances[0][0]}
            IF    '${t}' == '1'
                balance update    ${id}   ${asset}    -${balance}
            ELSE
                balance unfreeze    ${id}   ${asset}    ${balance}
            END
        END
    END
check balance
    [Arguments]  ${id}    ${asset}    ${t}    ${ret}
    ${timestamps}  mysql    select time from slice_history order by id desc limit 1
    Log    ${timestamps[0][0]}
    ${balances}    mysql    select balance from slice_balance_${timestamps[0][0]} where user_id = ${id} and asset = '${asset}' and t = ${t}
    ${balance}    Convert To Number    ${balances[0][0]}
    Should Be Equal As Numbers    ${balance}    ${ret}
check order
    [Arguments]  ${id}    ${oper_type}    ${side}    ${amount}
    ${timestamps}  mysql    select time from slice_history order by id desc limit 1
    ${actual_size}    Get Length    ${timestamps[0]}
    IF    '${actual_size}' == '0'
        Return From Keyword    0
    END
    ${orders}    mysql    select * from slice_order_${timestamps[0][0]} where user_id = ${id} and market = "BTCBCH" and oper_type = ${oper_type} and side = ${side}
    ${actual_size}    Get Length    ${orders}
    IF    '${actual_size}' != '0'
        ${amount_real}    Convert To Number    ${orders[0][10]}
        Should Be Equal As Numbers    ${amount_real}    ${amount}
    ELSE
        Should Be Equal As Numbers    0    ${amount}
    END
mysql
    [Arguments]  ${sql}
    Connect To Database Using Custom Params    pymysql    database='trade_log', user='root', password='pass', host='192.168.1.3', port=3306
    ${ret}    query    ${sql}
    Disconnect From Database
    [Return]    ${ret}
balance update
    [Arguments]  ${user}    ${asset}    ${amount}
    ${BUSSESSID}  Read Increment Variable    ${file_path}
    call    'balance.update'    ${user}, "${asset}", "deposit", ${BUSSESSID}, "${amount}", {"name": "anyone"}
    ${new_value}    Evaluate    ${BUSSESSID} + 1
    Write Increment Variable    ${file_path}    ${new_value}
balance unfreeze
    [Arguments]  ${user}    ${asset}    ${amount}
    call    'balance.unfreeze'    ${user}, "${asset}", "${amount}"
Read Increment Variable
    [Arguments]    ${file_path}
    ${value} =    test.read_increment_variable    ${file_path}
    [Return]    ${value}

Write Increment Variable
    [Arguments]    ${file_path}    ${value}
    test.write_increment_variable    ${file_path}    ${value}