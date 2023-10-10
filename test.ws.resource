*** Settings ***
Documentation     An example resource file
Library   WebSocketClient
Library   JSONLibrary
*** Variables ***


*** Keywords ***
wscall
    [Arguments]  ${method}    ${data}
    ${my_websocket} =  WebSocketClient.Connect  ws://127.0.0.1:8090
    WebSocketClient.Send  ${my_websocket}  {"method": "${method}","params": [${data}],"id": 1}
    ${result} =  WebSocketClient.Recv  ${my_websocket}
    ${result} =  To Json  ${result}
    WebSocketClient.Close  ${my_websocket}
    [Return]    ${result}
Echo
    ${ret} =  wscall    server.ping    ""
    Should Be Equal As Strings   pong   ${ret["result"]}
time
    ${ret} =  wscall    server.time    ""
kline.query
    ${ret} =  wscall    kline.query   ""