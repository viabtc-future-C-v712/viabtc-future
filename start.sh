#!/bin/bash
killall -s SIGQUIT accesshttp.exe
killall -s SIGQUIT matchengine.exe
killall -s SIGQUIT marketprice.exe

sleep 1
/viabtc-deal/bin/accesshttp.exe /viabtc-deal/accesshttp/config.json
/viabtc-deal/bin/matchengine.exe /viabtc-deal/matchengine/config.json
/viabtc-deal/bin/marketprice.exe /viabtc-deal/marketprice/config.json

tail -f /var/log/trade/matchengine.log /var/log/trade/marketprice.log /var/log/trade/accesshttp.log