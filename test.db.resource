*** Settings ***
Documentation     An example resource file
Library    DatabaseLibrary

*** Variables ***


*** Keywords ***
check db position
    [Arguments]  ${id}    ${side}    ${amount}
    ${timestamps}  mysql    select time from slice_history order by id desc limit 1
    ${actual_size}    Get Length    ${timestamps[0]}
    IF    '${actual_size}' == '0'
        Return From Keyword    0
    END
    ${positions}    mysql    select * from slice_position_${timestamps[0][0]} where user_id = ${id} and market = "BTCBCH" and side = ${side}
    ${actual_size}    Get Length    ${positions}
    IF    '${actual_size}' != '0'
        ${amount_real}    Convert To Number    ${positions[0][6]}
        Should Be Equal As Numbers    ${amount_real}    ${amount}
    ELSE
        Should Be Equal As Numbers    0    ${amount}
    END
check db balance
    [Arguments]  ${id}    ${asset}    ${t}    ${ret}
    ${timestamps}  mysql    select time from slice_history order by id desc limit 1
    Log    ${timestamps[0][0]}
    ${balances}    mysql    select balance from slice_balance_${timestamps[0][0]} where user_id = ${id} and asset = '${asset}' and t = ${t}
    ${balance}    Convert To Number    ${balances[0][0]}
    Should Be Equal As Numbers    ${balance}    ${ret}
check db order
    [Arguments]  ${id}    ${oper_type}    ${side}    ${amount}
    ${timestamps}  mysql    select time from slice_history order by id desc limit 1
    ${actual_size}    Get Length    ${timestamps[0]}
    IF    '${actual_size}' == '${可用余额}'
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
init db balance
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
mysql
    [Arguments]  ${sql}
    Connect To Database Using Custom Params    pymysql    database='trade_log', user='root', password='pass', host='192.168.1.3', port=3306
    ${ret}    query    ${sql}
    Disconnect From Database
    [Return]    ${ret}