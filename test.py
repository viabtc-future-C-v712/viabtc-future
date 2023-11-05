# -*- coding: utf-8 -*-
from confluent_kafka import Consumer, TopicPartition
import time

from robot.api.deco import keyword

class MyKeywords:
    @keyword
    def my_keyword(self, arg1, arg2):
        # 在这里实现你的关键字
        pass
        
def read_increment_variable(file_path):
    try:
        with open(file_path, 'r') as file:
            content = file.read()
            return int(content)
    except FileNotFoundError:
        return 1  # 如果文件不存在，返回初始值

def write_increment_variable(file_path, value):
    with open(file_path, 'w') as file:
        file.write(str(value))

def consume_from_offset(topic, partition=0, offset=0, timeout=0):
    consumer = Consumer({
        'bootstrap.servers': '127.0.0.1:9092',
        'group.id': 'mygroup',
        'auto.offset.reset': 'earliest'
    })
    tp = TopicPartition(topic, partition, offset)
    consumer.assign([tp])
    msg = consumer.poll(10.0)
    if msg is None:
        return "No message"
    elif msg.error():
        return "Consumer error: {}".format(msg.error())
    else:
        return msg.value().decode('utf-8')
    

def get_max_offset(topic, partition=0):
    consumer = Consumer({
        'bootstrap.servers': '127.0.0.1:9092',
        'group.id': 'mygroup',
        'auto.offset.reset': 'earliest'
    })

    tp = TopicPartition(topic, partition)
    high_watermark = consumer.get_watermark_offsets(tp)[1] - 1

    return high_watermark
