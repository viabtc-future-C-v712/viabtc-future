#!/bin/bash
pushd viabtc_exchange_server/matchengine
./restart.sh
popd

pushd viabtc_exchange_server/readhistory
./restart.sh
popd

pushd viabtc_exchange_server/accesshttp
./restart.sh
popd

pushd viabtc_exchange_server/accessws
./restart.sh
popd

pushd viabtc_exchange_server/alertcenter
./restart.sh
popd

sleep 20
pushd viabtc_exchange_server/marketprice
./restart.sh
popd
tail -f /dev/null