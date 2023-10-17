*** Settings ***
Documentation     An example resource file
Library   WebSocketClient
Library   JSONLibrary
*** Variables ***


*** Keywords ***
wscall start
    ${my_websocket} =  WebSocketClient.Connect  ws://127.0.0.1:8090
    [Return]    ${my_websocket}
wscall send
    [Arguments]  ${my_websocket}    ${method}    ${data}
    WebSocketClient.Send  ${my_websocket}  {"method": "${method}","params": [${data}],"id": 1}
wscall recv
    [Arguments]  ${my_websocket}
    ${result} =  WebSocketClient.Recv  ${my_websocket}
    ${result} =  To Json  ${result}
    [Return]    ${result}
wscall end
    [Arguments]  ${my_websocket}
    WebSocketClient.Close  ${my_websocket}
Echo
    ${ret} =  wscall    server.ping    ""
    Should Be Equal As Strings   pong   ${ret["result"]}
time
    ${ret} =  wscall    server.time    ""
kline.query
    ${ret} =  wscall    kline.query   ""
position.query
    [Arguments]  ${id}    ${market}=BTCBCH    ${side}=1
    ${ret} =  wscall    position.query   ${id}, "${market}", ${side}
