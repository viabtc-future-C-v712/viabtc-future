# -*- coding: utf-8 -*-

BASE_URL             = 'http://127.0.0.1:8080/'
file_path            =  'file.txt'
BOOTSTRAP_SERVERS    = '127.0.0.1:9092'
Alice                = 1
Bob                  = 2
可用余额              = 1
冻结余额              = 2
空                   = 1
多                   = 2
市价                 = 0
限价                 = 1
逐仓                 = 1
全仓                 = 2
平仓                 = 2
开仓                 = 1
可用仓位              = 'position'
冻结仓位              = 'frozen'

# 创建列表变量
pylist = ["大宗商品", "供应链金融", '2C电商']

# 创建字典变量
pydict = {'username': 'Brian', 'passwd': '20171219'}

# 创建集合变量
pyset = {"sing", 'jump'}

# 创建元组变量
pytuple = ("age", "name", "sex")

# 创建保护变量（以下划线开头的保护变量不会被RF导入）
_pykey = "f007688f401311e782617cd30ab49afc"