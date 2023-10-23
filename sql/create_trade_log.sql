CREATE DATABASE IF NOT EXISTS trade_log;
USE trade_log;
CREATE TABLE `slice_balance_example` (
    `id`            INT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
    `user_id`       INT UNSIGNED NOT NULL,
    `asset`         VARCHAR(30) NOT NULL,
    `t`             TINYINT UNSIGNED NOT NULL, -- 1 available, 2 frozen
    `balance`       DECIMAL(35,16) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `slice_market_example` (
    `id`            INT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
    `name`          VARCHAR(30) NOT NULL,
    `stock_name`    VARCHAR(30) NOT NULL,
    `stock_prec`    TINYINT UNSIGNED NOT NULL,
    `money_name`    VARCHAR(30) NOT NULL,
    `money_prec`    TINYINT UNSIGNED NOT NULL,
    `min_amount`    DECIMAL(35,16) NOT NULL,
    `price`         DECIMAL(35,16) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `slice_position_example` (
    `id`            INT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
    `user_id`       INT UNSIGNED NOT NULL,
    `market`        VARCHAR(30) NOT NULL,
    `side`          TINYINT UNSIGNED NOT NULL, -- 1 卖, 2 买
    `pattern`       TINYINT UNSIGNED NOT NULL, -- 1 逐仓, 2 全仓
    `leverage`      DECIMAL(8,2) NOT NULL, -- 杠杆倍数
    `position`      DECIMAL(35,16) NOT NULL, -- 仓位 （保证金X杠杆）
    `frozen`        DECIMAL(35,16) NOT NULL, -- 冻结的仓位
    `price`         DECIMAL(35,16) NOT NULL, -- 成交价
    `principal`     DECIMAL(35,16) NOT NULL -- 保证金
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `slice_order_example` (
    `id`            BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `t`             TINYINT UNSIGNED NOT NULL, -- 1 limit, 2 market, 3 trigger(price=0表示为market)
    `isblast`      TINYINT UNSIGNED NOT NULL, -- 0 非爆仓单，1 爆仓单
    `side`          TINYINT UNSIGNED NOT NULL, -- 1 卖, 2 买
    `pattern`       TINYINT UNSIGNED NOT NULL, -- 1 逐仓, 2 全仓
    `oper_type`     TINYINT UNSIGNED NOT NULL, -- 1 open, 2 close
    `create_time`   DOUBLE NOT NULL,
    `update_time`   DOUBLE NOT NULL,
    `user_id`       INT UNSIGNED NOT NULL,
    `market`        VARCHAR(30) NOT NULL,
    `relate_order`  BIGINT UNSIGNED NOT NULL, -- relate order id for tp & sl
    `price`         DECIMAL(35,16) NOT NULL,
    `amount`        DECIMAL(35,16) NOT NULL, -- 交易的单位 U 既 volume
    `leverage`      DECIMAL(8,2) NOT NULL,
    `trigger`       DECIMAL(35,16) NOT NULL, -- trigger price
    `taker_fee`     DECIMAL(8,6) NOT NULL,
    `maker_fee`     DECIMAL(8,6) NOT NULL,
    `left`          DECIMAL(35,16) NOT NULL, -- 买卖的单位 U 既 volume
    `freeze`        DECIMAL(35,16) NOT NULL, -- 买卖的单位 U 既 volume
    `deal_stock`    DECIMAL(35,16) NOT NULL, -- 已交易的币
    `deal_money`    DECIMAL(35,16) NOT NULL, -- 已交易的余额
    `deal_fee`      DECIMAL(35,16) NOT NULL, -- 已交易的费用
    `source`        VARCHAR(30) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `slice_history` (
    `id`            INT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
    `time`          BIGINT NOT NULL,
    `end_oper_id`   BIGINT UNSIGNED NOT NULL,
    `end_order_id`  BIGINT UNSIGNED NOT NULL,
    `end_deals_id`  BIGINT UNSIGNED NOT NULL,
    `end_position_id`  BIGINT UNSIGNED NOT NULL,
    `end_market_id`  BIGINT UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `operlog_example` (
    `id`            BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `time`          DOUBLE NOT NULL,
    `detail`        TEXT
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
