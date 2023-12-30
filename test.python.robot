*** Settings ***
Library    OperatingSystem
Library    test
Resource   test.http.resource
Variables  test_variable.py

*** Test Cases ***
position mode query
    init balance    ${Alice}    BCH
    balance update    ${Alice}    BCH    800000
    check balance    ${Alice}    BCH    ${可用余额}    800000
    