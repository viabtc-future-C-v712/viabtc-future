#!/bin/bash
killall -s SIGQUIT accesshttp.exe
killall -s SIGQUIT accessws.exe
killall -s SIGQUIT matchengine.exe
killall -s SIGQUIT marketprice.exe
killall -s SIGQUIT readhistory.exe

sleep 5

/viabtc-deal/bin/matchengine.exe /viabtc-deal/matchengine/config.json
/viabtc-deal/bin/readhistory.exe /viabtc-deal/readhistory/config.json
/viabtc-deal/bin/accesshttp.exe /viabtc-deal/accesshttp/config.json
/viabtc-deal/bin/accessws.exe /viabtc-deal/accessws/config.json
sleep 5
/viabtc-deal/bin/marketprice.exe /viabtc-deal/marketprice/config.json

pushd /viabtc-deal/httpsServer
nohup python3 server.py & 
popd

tail -f /var/log/trade/matchengine.log /var/log/trade/marketprice.log /var/log/trade/accesshttp.log /var/log/trade/accessws.log /var/log/trade/readhistory.log