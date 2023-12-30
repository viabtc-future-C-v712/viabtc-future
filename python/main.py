import argparse
from decimal import Decimal
from enum import Enum
import http.server
import ssl
import json
import urllib

class User:
    def __init__(self, id:int):
        self.id = id
    def my_method(self):
        print("Hello, World!")

gUserList = []

class Asset:
    def __init__(self):
        self.moneys = {}
    def a(self):
        print("a")

gAssetList = {}

class Position:
    def a(self):
        print("a")

gPositionList = []

class Type(Enum):
    MARKET = 0
    LIMIT = 1
    PLAN = 2
    TPSL = 3

class OrderArgs:
    def __init__(self, post_data):
        if(len(post_data) == 12):
            self.bOpen = True
            self.user_id = post_data[0]
            self.market_name = post_data[1]
            self.direction = post_data[2]
            self.type = post_data[3]
            self.pattern = post_data[4]
            self.markPrice = post_data[5]
            self.triggerPrice = post_data[6]
            self.entrustPrice = post_data[7]
            self.leverage = post_data[8]
            self.volume = post_data[9]
            self.taker_fee = post_data[10]
            self.maker_fee = post_data[11]
        else:
            self.bOpen = False
            self.user_id = post_data[0]
            self.market_name = post_data[1]
            self.direction = post_data[2]
            self.type = post_data[3]
            self.pattern = post_data[4]
            self.markPrice = post_data[5]
            self.triggerPrice = post_data[6]
            self.entrustPrice = post_data[7]
            self.volume = post_data[8]
            self.taker_fee = post_data[9]
            self.maker_fee = post_data[10]
            self.tpPrice = post_data[11]
            self.tpAmount = post_data[12]
            self.slPrice = post_data[13]
            self.slAmount = post_data[14]

class Order:
    def __init__(self, args: OrderArgs):
        self.args = args

class OrderMarket(Order):
    def __init__(self, args: OrderArgs):
        super().__init__(args)

class OrderLimit(Order):
    def __init__(self, args: OrderArgs):
        super().__init__(args)

class OrderPlan(Order,):
    def __init__(self, args: OrderArgs):
        super().__init__(args)

class OrderTpsl(Order):
    def __init__(self, args: OrderArgs):
        super().__init__(args)

class Market():
    def __init__(self):
        super().__init__()
        self.OrderAskList = []
        self.OrderBidList = []
        self.MatchOrderAskList = []
        self.MatchOrderBidList = []
    def Match(self, order: Order):
        pass

gMarketList = {}

class Balance:
    def __init__(self):
        self.data = {}
    def deposit(self, user:User, money: list):
        if user.id in gAssetList:
            sum = int(gAssetList[user.id].moneys[money[0]]["available"]) + int(money[1])
            gAssetList[user.id].moneys[money[0]]["available"] = f"{sum}"
        else:
            gAssetList[user.id] = Asset()
            gAssetList[user.id].moneys[money[0]] = {"available": money[1], "freeze": '0'}
    def withdrawal(self, user:User, money: list):
        pass
    def query(self, post_data):
        user = User(post_data[0])
        money = post_data[1]
        if user.id in gAssetList:
            self.data[money] = {"available":gAssetList[user.id].moneys[money]["available"], "freeze": gAssetList[user.id].moneys[money]["freeze"]}
        else:
            self.data[money] = {"available":'0', "freeze":'0'}
        print(post_data)
    def response(self):
        return self.data

class Match:
    global gUserList
    global gAssetList
    global gPositionList
    global gMarketList
    def check(self):
        pass
    def process(self, order: Order):
        if order.args.market_name not in gMarketList:
            gMarketList[order.args.market_name] = Market()
        market = gMarketList[order.args.market_name]
        return market.Match(order)
        

class MatchMarket(Match):
    def __init__(self):
        super().__init__()
    def process(self, order: Order):
        oOrder = super.process(order)
                                            

class MatchLimit(Match):
    def __init__(self):
        super().__init__()

class MatchPlan(Match):
    def __init__(self):
        super().__init__()

def getOrder(post_data) -> Order:
    params = post_data["params"]
    orderArgs = OrderArgs(params)
    if orderArgs.type == Type.MARKET:
        order = OrderMarket(orderArgs)
    elif orderArgs.type == Type.LIMIT:
        order = OrderLimit(orderArgs)
    elif orderArgs.type == Type.PLAN:
        order = OrderPlan(orderArgs)
    elif orderArgs.type == Type.TPSL:
        order = OrderTpsl(orderArgs)
    return order

def getCmd(post_data):
    if post_data["method"] == "order.open":
        order = getOrder(post_data)
        return order
    elif post_data["method"] == "balance.update":
        balance = Balance()
        if post_data["params"][2] == 'deposit':
            balance.deposit(User(post_data["params"][0]), [post_data["params"][1], post_data["params"][4]])
        return balance
    elif post_data["method"] == "balance.query":
        balance = Balance()
        balance.query(post_data["params"])
        return balance
    else:
        print(post_data)
        return None

matchMarket = MatchMarket()
matchLimit = MatchLimit()
matchPlan = MatchPlan()

class HttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        query = urllib.parse.urlparse(self.path).query
        post_data = self.rfile.read(int(self.headers.get('Content-Length')))
        requestData = json.loads(post_data)

        cmd = getCmd(requestData)
        if type(cmd) == Order:
            if cmd.type == Type.MARKET:
                matchMarket.process(cmd)
            elif cmd.type == Type.LIMIT:
                matchLimit.process(cmd)
            elif cmd.type == Type.PLAN:
                matchPlan.process(cmd)
        elif type(cmd) == Balance:
            data = cmd.response()

        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        response = {"code": 0, "message": "null", "result": data,"error": None}
        self.wfile.write(json.dumps(response).encode())

def main():
    server_address = ('', 8000)
    httpd = http.server.HTTPServer(server_address, HttpRequestHandler)
    httpd.serve_forever()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='viabtc')
    parser.add_argument('--env', required=True, help='Name of the environment')
    main()