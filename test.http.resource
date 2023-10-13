*** Settings ***
Documentation     An example resource file
Library    RequestsLibrary
Library    test
Resource   test.kafka.resource
Variables  test_variable.py

*** Variables ***


*** Keywords ***
put limit
    [Arguments]  ${data}
    call    'order.put_limit'    ${data}
put open
    [Arguments]  ${用户}    ${买或卖}    ${市价或限价}     ${逐仓或全仓}    ${张}    ${价格}=8000
    call    'order.open'    ${用户}, "BTCBCH", ${买或卖}, ${市价或限价}, ${逐仓或全仓}, "8000", "8000", "${价格}", "100", "${张}", "0.002", "0.001"
put close
    [Arguments]  ${用户}    ${买或卖}    ${市价或限价}     ${逐仓或全仓}    ${张}    ${价格}=8000
    call    'order.close'    ${用户}, "BTCBCH", ${买或卖}, ${市价或限价}, ${逐仓或全仓}, "8000", "8000", "${价格}", "${张}", "0.002", "0.001"
make slice
    call    'matchengine.makeslice'    ""
    sleep    1
init balance all
    init balance    ${Alice}    BCH
    balance update    ${Alice}    BCH    800000
    ${max_offset}=    Evaluate    test.get_max_offset('balances')
    kafka balances    ${Alice}    800000    ${max_offset}
    init balance    ${Bob}    BCH
    balance update    ${Bob}    BCH    800000
    ${max_offset}=    Evaluate    test.get_max_offset('balances')
    kafka balances    ${Bob}    800000    ${max_offset}
    init balance    ${Carol}    BCH
    balance update    ${Carol}    BCH    800000
    ${max_offset}=    Evaluate    test.get_max_offset('balances')
    kafka balances    ${Carol}    800000    ${max_offset}
init balance
    [Arguments]  ${id}    ${asset}
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
    [Arguments]  ${id}    ${amount}=0    ${market}=BTCBCH    ${offset}=0    ${limit}=1
    ${ret} =  call    'order.pending'    ${id}, "${market}", ${offset}, ${limit}
    IF    '${ret.json()["result"]["total"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        Should Be Equal As Numbers    ${ret.json()["result"]["records"][0]["left"]}    ${amount}
    END
check order brief
    [Arguments]  ${id}    ${amount}=0    ${market}=BTCBCH    ${offset}=0    ${limit}=1
    ${ret} =  call    'order.pending_brief'    ${id}, "${market}", ${offset}, ${limit}
    IF    '${ret.json()["result"]["total"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        Should Be Equal As Numbers    ${ret.json()["result"]["records"][0]["amount"]}    ${amount}
    END
    # [Return]    ${ret.json()["result"]["records"][0]["id"]}
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
check position
    [Arguments]  ${id}    ${side}    ${type}    ${amount}
    ${ret} =  call    'position.query'    ${id}, "BTCBCH", ${side}, 
    IF    '${ret.json()["result"]["count"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        IF    '${amount}' == '0'
            Should Be Equal As Numbers    ${ret.json()["result"]["BTCBCH"]["${type}"]}    0
        ELSE
            Should Be Equal As Numbers    ${ret.json()["result"]["BTCBCH"]["${type}"]}    ${amount}
        END
    END
market last
    call    'market.last'    "BTCBCH"
重启
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