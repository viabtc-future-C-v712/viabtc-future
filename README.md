# viabtc-future
* VERSION:  V0.0.1
* DATE:     2023-09-25

## 简介
viabtc-future 参考viabtc exchange server 开发的合约撮合服务


## start 
docker-compose -f ../docker-compose.yml up -d

## down
docker-compose -f ../docker-compose.yml down

## 打开bash
docker-compose exec viabtcdeal bash

## 运行
./start.sh

## 宿主机 /etc/hosts 添加 host.docker.internal域名，用于在docker中访问