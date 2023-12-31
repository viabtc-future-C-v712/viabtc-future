*** Settings ***
Documentation     An example resource file
Library    RequestsLibrary
Library    test
Resource   test.kafka.resource
Resource   test.db.resource
Variables  test_variable.py

*** Variables ***


*** Keywords ***
put limit
    [Arguments]  ${data}
    call    'order.put_limit'    ${data}
市价逐仓开多
    [Arguments]  ${用户}    ${张}    ${market}=BTCBCH
    put open     ${用户}    ${多}    ${市价}    ${逐仓}    ${张}    价格=8001    market=${market}  #成交后的市场价格为8001
市价逐仓开空
    [Arguments]  ${用户}    ${张}    ${market}=BTCBCH
    put open     ${用户}    ${空}    ${市价}    ${逐仓}    ${张}    价格=9000    market=${market}  #价格高于触发价后下单，
put open
    [Arguments]  ${用户}    ${买或卖}    ${市价或限价}     ${逐仓或全仓}    ${张}    ${价格}=8000    ${触发价格}=8000    ${market}=BTCBCH
    call    'order.open'    ${用户}, "${market}", ${买或卖}, ${市价或限价}, ${逐仓或全仓}, "0", "${触发价格}", "${价格}", "100", "${张}", "0.002", "0.001"
止盈止损逐仓平空
    [Arguments]  ${用户}    ${market}=BTCBCH    ${tpPrice}=7999    ${tpAmount}=10000    ${slPrice}=8001    ${slAmount}=10000
    put close     ${用户}    ${空}    ${止盈止损}    ${逐仓}    0    价格=0    触发价格=8000   market=${market}    tpPrice=${tpPrice}    tpAmount=${tpAmount}    slPrice=${slPrice}    slAmount=${slAmount}
put close
    [Arguments]  ${用户}    ${买或卖}    ${市价或限价}     ${逐仓或全仓}    ${张}    ${价格}=8000    ${触发价格}=8000    ${market}=BTCBCH    ${tpPrice}=7999    ${tpAmount}=10000    ${slPrice}=8001    ${slAmount}=10000
    call    'order.close'    ${用户}, "${market}", ${买或卖}, ${市价或限价}, ${逐仓或全仓}, "8000", "${触发价格}", "${价格}", "${张}", "0.002", "0.001", "${tpPrice}", "${tpAmount}", "${slPrice}", "${slAmount}"
make slice
    call    'matchengine.makeslice'    ""
    sleep    1
init balance all
    [Arguments]  ${asset}=BCH
    init balance    ${Alice}    ${asset}
    balance update    ${Alice}    ${asset}    800000
    ${max_offset}=    Evaluate    test.get_max_offset('balances')
    kafka balances    ${Alice}    800000    ${max_offset}
    init balance    ${Bob}    ${asset}
    balance update    ${Bob}    ${asset}    800000
    ${max_offset}=    Evaluate    test.get_max_offset('balances')
    kafka balances    ${Bob}    800000    ${max_offset}
    init balance    ${Carol}    ${asset}
    balance update    ${Carol}    ${asset}    800000
    ${max_offset}=    Evaluate    test.get_max_offset('balances')
    kafka balances    ${Carol}    800000    ${max_offset}
init balance
    [Arguments]  ${id}    ${asset}=BCH
    ${ret} =  call    'balance.query'    ${id}, "${asset}"
    IF    '${ret.json()["result"]["${asset}"]["freeze"]}' != '0'
        balance unfreeze    ${id}    ${asset}    ${ret.json()["result"]["${asset}"]["freeze"]}
        ${ret} =  call    'balance.query'    ${id}, "${asset}"
    END
    IF    '${ret.json()["result"]["${asset}"]["available"]}' != '0'
        balance update    ${id}    ${asset}    -${ret.json()["result"]["${asset}"]["available"]}
    END
    ${ret} =  call    'balance.query'    ${id}, "${asset}"
    Should Be Equal As Numbers    ${ret.json()["result"]["${asset}"]["freeze"]}    0
    Should Be Equal As Numbers    ${ret.json()["result"]["${asset}"]["available"]}    0
check balance
    [Arguments]  ${id}    ${asset}    ${t}    ${amount}
    ${ret} =  call    'balance.query'    ${id}, "${asset}"
    IF    '${t}' == '${可用余额}'
        Should Be Equal As Numbers    ${ret.json()["result"]["${asset}"]["available"]}    ${amount}
    ELSE
        Should Be Equal As Numbers    ${ret.json()["result"]["${asset}"]["freeze"]}    ${amount}
    END
check order
    [Arguments]  ${id}    ${amount}=0    ${market}=BTCBCH    ${type}=${限价}    ${offset}=0    ${limit}=100   ${index}=0
    ${ret} =  call    'order.pending'    ${id}, "${market}", ${offset}, ${limit}
    IF    '${ret.json()["result"]["total"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        Should Be Equal As Numbers    ${ret.json()["result"]["records"][${index}]["left"]}    ${amount}
        Should Be Equal As Numbers    ${ret.json()["result"]["records"][${index}]["type"]}    ${type}
    END
check order brief
    [Arguments]  ${id}    ${amount}=0    ${market}=BTCBCH    ${offset}=0    ${limit}=1
    ${ret} =  call    'order.pending_brief'    ${id}, "${market}", ${offset}, ${limit}
    IF    '${ret.json()["result"]["total"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        Should Be Equal As Numbers    ${ret.json()["result"]["records"][0]["amount"]}    ${amount}
    END
    [Return]    ${ret.json()["result"]["records"][0]["id"]}
check order detail
    [Arguments]  ${market}=BTCBCH    ${amount}=0    ${orderid}=1
    ${ret} =  call    'order.pending_detail'    "${market}", ${orderid}
    Should Be Equal As Numbers    ${ret.json()["result"]["left"]}    ${amount}
check order all
    [Arguments]  ${id}    ${amount}=0    ${offset}=0    ${limit}=1
    ${ret} =  call    'order.pending_all'    ${id}, ${offset}, ${limit}
    IF    '${ret.json()["result"]["total"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        Should Be Equal As Numbers    ${ret.json()["result"]["records"][0]["left"]}    ${amount}
    END
check order alluser
    [Arguments]  ${amount}=0    ${offset}=0    ${limit}=1
    ${ret} =  call    'order.pending_alluser'    ${offset}, ${limit}
    IF    '${ret.json()["result"]["total"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        Should Be Equal As Numbers    ${ret.json()["result"]["records"][0]["left"]}    ${amount}
    END
check order book
    [Arguments]  ${market}=BTCBCH    ${side}=2    ${offset}=0    ${limit}=1
    ${ret} =  call    'order.book'    "${market}", ${side}, ${offset}, ${limit}
check order depth
    [Arguments]  ${market}=BTCBCH    ${limit}=10    ${interval}=0.1
    ${ret} =  call    'order.depth'    "${market}", ${limit}, "${interval}"
order cancel
    [Arguments]  ${id}    ${market}=BTCBCH    ${orderid}=1
    ${ret} =  call    'order.cancel'    ${id}, "${market}", ${orderid}
balance update
    [Arguments]  ${user}    ${asset}    ${amount}
    ${BUSSESSID}  Read Increment Variable    ${file_path}
    call    'balance.update'    ${user}, "${asset}", "deposit", ${BUSSESSID}, "${amount}", {"name": "anyone"}
    ${new_value}    Evaluate    ${BUSSESSID} + 1
    Write Increment Variable    ${file_path}    ${new_value}
balance unfreeze
    [Arguments]  ${user}    ${asset}    ${amount}
    call    'balance.unfreeze'    ${user}, "${asset}", "${amount}"
检查空仓冻结
    [Arguments]  ${id}    ${amount}=0    ${market}=BTCBCH
    check position    ${id}    market=${market}    side=${空}    type=${冻结仓位}    amount=${amount}
检查空仓可用
    [Arguments]  ${id}    ${amount}=0    ${market}=BTCBCH
    check position    ${id}    market=${market}    side=${空}    type=${可用仓位}    amount=${amount}
check position
    [Arguments]  ${id}    ${market}=BTCBCH    ${side}=${多}    ${type}=${可用仓位}    ${amount}=0
    ${ret} =  call    'position.query'    ${id}, "${market}", ${side}
    IF    '${ret.json()["result"]["count"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        IF    '${amount}' == '0'
            Should Be Equal As Numbers    ${ret.json()["result"]["positions"][0]["${type}"]}    0
        ELSE
            Should Be Equal As Numbers    ${ret.json()["result"]["positions"][0]["${type}"]}    ${amount}
        END
    END
check position mode
    [Arguments]  ${id}    ${market}=BTCBCH    ${mode}=1    ${leverage}=1
    ${ret} =  call    'position_mode.query'    ${id}, "${market}"
    Should Be Equal As Numbers    ${ret.json()["result"]["position_modes"][0]["pattern"]}    ${mode}
    Should Be Equal As Numbers    ${ret.json()["result"]["position_modes"][0]["leverage"]}    ${leverage}
position mode adjust
    [Arguments]  ${id}    ${market}=BTCBCH    ${mode}=1    ${leverage}=10
    ${ret} =  call    'position_mode.adjust'    ${id}, "${market}", ${mode}, "${leverage}"
check position all
    [Arguments]  ${id}    ${market}=BTCBCH    ${type}=${可用仓位}    ${amount}=0
    ${ret} =  call    'position.query_all'    ${id}, "${market}"
    IF    '${ret.json()["result"]["count"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        IF    '${amount}' == '0'
            Should Be Equal As Numbers    ${ret.json()["result"]["postions"][0]["${type}"]}    0
        ELSE
            Should Be Equal As Numbers    ${ret.json()["result"]["postions"][0]["${type}"]}    ${amount}
        END
    END
position update
    [Arguments]  ${id}    ${side}    ${amount}
    ${ret} =  call    'position.adjust_principal'    ${id}, "BTCBCH", ${side}, "${amount}"
market last
    call    'market.last'    "BTCBCH"
重启服务 并清理数据库
    sleep    0.2s
    init oper Log
    call    'matchengine.suicide'    -1
重启服务
    sleep    0.2s
    call    'matchengine.suicide'    -1
退出
    call    'matchengine.suicide'    0
call
    [Arguments]  ${method}    ${data}
    Create Session    user    ${BASE_URL}
    ${data2} =  Evaluate  {'method': ${method}, 'params': [${data}], 'id': 3}

    ${resp} =    POST On Session    user    /    json=${data2}
    Should Be Equal As Strings    ${resp.status_code}    200
    Should Be Equal As Strings    ${resp.json()["error"]}    None
    [Return]    ${resp}
Read Increment Variable
    [Arguments]    ${file_path}
    ${value} =    test.read_increment_variable    ${file_path}
    [Return]    ${value}
Write Increment Variable
    [Arguments]    ${file_path}    ${value}
    test.write_increment_variable    ${file_path}    ${value}