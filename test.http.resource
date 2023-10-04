*** Settings ***
Documentation     An example resource file
Library    RequestsLibrary
Library    test

*** Variables ***
${BASE_URL}             http://192.168.1.3:8080/
${file_path}            file.txt
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

*** Keywords ***
put limit
    [Arguments]  ${data}
    call    'order.put_limit'    ${data}
put open
    [Arguments]  ${用户}    ${买或卖}    ${市价或限价}     ${逐仓或全仓}    ${张}
    call    'order.open'    ${用户}, "BTCBCH", ${买或卖}, ${市价或限价}, ${逐仓或全仓}, "8000", "8000", "8000", "100", "${张}", "0.002", "0.001"
put close
    [Arguments]  ${用户}    ${买或卖}    ${市价或限价}     ${逐仓或全仓}    ${张}
    call    'order.close'    ${用户}, "BTCBCH", ${买或卖}, ${市价或限价}, ${逐仓或全仓}, "8000", "8000", "8000", "${张}", "0.002", "0.001"
make slice
    call    'matchengine.makeslice'    ""
    sleep    1
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
    [Arguments]  ${id}    ${oper_type}    ${side}    ${amount}
    ${ret} =  call    'order.pending'    ${id}, "BTCBCH", 0, 2
    IF    '${amount}' == '0'
        Should Be Equal As Numbers    ${ret.json()["result"]["total"]}    0
    END
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
    [Arguments]  ${id}    ${side}    ${amount}
    ${ret} =  call    'position.query'    ${id}, "BTCBCH", ${side}, 
    IF    '${ret.json()["result"]["count"]}' == '0'
        Should Be Equal As Numbers    0    ${amount}
    ELSE
        IF    '${amount}' == '0'
            Should Be Equal As Numbers    ${ret.json()["result"]["BTCBCH"]["position"]}    0
        ELSE
            Should Be Equal As Numbers    ${ret.json()["result"]["BTCBCH"]["position"]}    ${amount}
        END
    END
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