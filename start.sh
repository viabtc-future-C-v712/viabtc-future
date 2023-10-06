#!/bin/bash
killall -s SIGQUIT accesshttp.exe
killall -s SIGQUIT matchengine.exe

sleep 1
/viabtc-deal/bin/accesshttp.exe /viabtc-deal/accesshttp/config.json
/viabtc-deal/bin/matchengine.exe /viabtc-deal/matchengine/config.json

tail -f /var/log/trade/matchengine.log