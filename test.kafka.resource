*** Settings ***
Library    test
Variables  test_variable.py

*** Keywords ***
kafka orders
    [Arguments]    ${id}    ${value}    ${max_offset}
    ${messages}=    Evaluate    test.consume_from_offset('orders', 0, ${max_offset})
    ${event} =  Evaluate  ${messages}\['event'\]
    ${order} =  Evaluate  ${messages}\['order'\]
    ${idValue} =  Evaluate  ${order}\['user'\]
    Should Be Equal As Numbers    ${idValue}    ${id}
    ${amount} =  Evaluate  ${order}\['amount'\]
    Should Be Equal As Numbers    ${amount}    ${value}
kafka balances
    [Arguments]    ${id}    ${value}    ${max_offset}
    ${messages}=    Evaluate    test.consume_from_offset('balances', 0, ${max_offset}, 10000)
    ${itemValue} =  Evaluate  ${messages}\[4\]
    ${idValue} =  Evaluate  ${messages}\[1\]
    Should Be Equal As Numbers    ${idValue}    ${id}
    Should Be Equal As Numbers    ${itemValue}    ${value}
kafka deals
    [Arguments]    ${卖id}    ${买id}    ${value}    ${max_offset}
    ${deals}=    Evaluate    test.consume_from_offset('deals', 0, ${max_offset})
    ${卖idValue} =  Evaluate  ${deals}\[4\]
    Should Be Equal As Numbers    ${卖idValue}    ${卖id}
    ${买idValue} =  Evaluate  ${deals}\[5\]
    Should Be Equal As Numbers    ${买idValue}    ${买id}
    ${itemValue} =  Evaluate  ${deals}\[7\]
    Should Be Equal As Numbers    ${itemValue}    ${value}
kafka positions
    [Arguments]    ${id}    ${max_offset}
    ${deals}=    Evaluate    test.consume_from_offset('positions', 0, ${max_offset})
kafka positions empty
    [Arguments]    ${max_offset}
    ${positions}=    Evaluate    test.consume_from_offset('positions', 0, ${max_offset})
    Should Be Equal As Strings    ${positions}    No message
kafka deals empty
    [Arguments]    ${max_offset}
    ${deals}=    Evaluate    test.consume_from_offset('deals', 0, ${max_offset})
    Should Be Equal As Strings    ${deals}    No message