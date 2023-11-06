
FROM viabtcdealbase

COPY . /usr/src/myapp
COPY ./.gdbinit /root
WORKDIR /usr/src/myapp

RUN apt-get update

# RUN apt-get -y install gcc g++ cmake make git wget libjansson-dev libssl-dev libev-dev libmysqlclient-dev libcurl4-nss-dev liblz4-dev mysql-client gdb redis-tools python python-pip python3 python3-pip iputils-ping psmisc nginx software-properties-common curl
# RUN pip install requests==2.8.1
# RUN pip3 install websockets==4.0.1
# RUN wget https://www.bytereef.org/software/mpdecimal/releases/mpdecimal-2.5.1.tar.gz
# RUN cd depends/hiredis && make install && cd ../../
# RUN tar -xf mpdecimal-2.5.1.tar.gz && cd mpdecimal-2.5.1 && ./configure && make -j install && cd ..
# RUN mkdir -p /var/log/trade
# git clone https://github.com/confluentinc/librdkafka.git
# RUN export http_proxy=socks5://172.26.96.209:1082 && export https_proxy=socks5://172.26.96.209:1082 && cd librdkafka && mkdir build && cd build && cmake .. -DRDKAFKA_BUILD_STATIC=ON && make -j && make install && cd ../../
# This command runs your application, comment out this line to compile only
#CMD ["sleep 100"]
# ENTRYPOINT ./start.sh
LABEL Name=viabtcdeal Version=0.0.1
