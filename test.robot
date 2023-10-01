*** Settings ***
Library    RequestsLibrary
Library    RedisLibrary
Library    ConfluentKafkaLibrary

*** Variables ***
${BASE_URL}    http://192.168.1.3:8080/
${BOOTSTRAP_SERVERS}    192.168.1.3:9092
${GROUP_ID}    test-group
*** Test Cases ***
test init
    init
test put open limit
    put open limit
test 1
    init
    put open limit
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
    balance update    1, "BCH", "deposit", 1, "800000", {"name": "Alice", "age": 25, "gender": "female"}
    balance update    2, "BTC", "deposit", 1, "100", {"name": "Alice", "age": 25, "gender": "female"}
    balance update    2, "BCH", "deposit", 1, "800000", {"name": "Alice", "age": 25, "gender": "female"}
    balance update    1, "BTC", "deposit", 1, "100", {"name": "Alice", "age": 25, "gender": "female"}
put open limit
    put open     1, "BTCBCH", 1, 1, 1, "8000", "8000", "8000", "100", "10000", "0.002", "0.001"
    put open     2, "BTCBCH", 2, 1, 1, "8000", "8000", "8000", "100", "10000", "0.002", "0.001"
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

balance update
    [Arguments]  ${data}
    call    'balance.update'    ${data}
put limit
    [Arguments]  ${data}
    call    'order.put_limit'    ${data}
put open
    [Arguments]  ${data}
    call    'order.open'    ${data}
call
    [Arguments]  ${method}    ${data}
    Create Session    user    ${BASE_URL}
    ${data2} =  Evaluate  {'method': ${method}, 'params': [${data}], 'id': 3}
    ${resp}=    POST On Session    user    /    json=${data2}
    Should Be Equal As Strings    ${resp.status_code}    200
    Should Be Equal As Strings    ${resp.json()["error"]}    None
