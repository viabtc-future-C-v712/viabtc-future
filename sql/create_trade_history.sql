CREATE DATABASE IF NOT EXISTS trade_history;
USE trade_history;
CREATE TABLE `balance_history_example` (
    `id`            BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
    `time`          DOUBLE NOT NULL,
    `user_id`       INT UNSIGNED NOT NULL,
    `asset`         VARCHAR(30) NOT NULL,
    `business`      VARCHAR(30) NOT NULL,
    `change`        DECIMAL(35,16) NOT NULL,
    `balance`       DECIMAL(35,16) NOT NULL,
    `detail`        TEXT NOT NULL,
    INDEX `idx_user_asset` (`user_id`, `asset`),
    INDEX `idx_user_asset_business` (`user_id`, `asset`, `business`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `position_history_example` (
    `id`            BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
    `time`          DOUBLE NOT NULL,
    `user_id`       INT UNSIGNED NOT NULL,
    `market`        VARCHAR(30) NOT NULL,
    `side`          TINYINT UNSIGNED NOT NULL, -- 1 卖, 2 买
    `pattern`       TINYINT UNSIGNED NOT NULL, -- 1 逐仓, 2 全仓
    `leverage`      DECIMAL(8,2) NOT NULL,
    `change`        DECIMAL(35,16) NOT NULL,
    `position`      DECIMAL(35,16) NOT NULL,
    `price`         DECIMAL(35,16) NOT NULL,
    `principal`     DECIMAL(35,16) NOT NULL,
    INDEX `idx_user_asset` (`user_id`, `market`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- split by user_id
CREATE TABLE `order_history_example` (
    `id`            BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `create_time`   DOUBLE NOT NULL,
    `finish_time`   DOUBLE NOT NULL,
    `user_id`       INT UNSIGNED NOT NULL,
    `market`        VARCHAR(30) NOT NULL,
    `source`        VARCHAR(30) NOT NULL,
    `t`             TINYINT UNSIGNED NOT NULL, -- 1 limit, 2 market, 3 trigger
    `side`          TINYINT UNSIGNED NOT NULL, -- 1 卖, 2 买
    `oper_type`     TINYINT UNSIGNED NOT NULL, -- 1 open, 2 close
    `price`         DECIMAL(35,16) NOT NULL,
    `amount`        DECIMAL(35,16) NOT NULL,
    `leverage`      DECIMAL(8,2) NOT NULL,
    `trigger`       DECIMAL(35,16) NOT NULL, -- trigger price
    `taker_fee`     DECIMAL(8,6) NOT NULL,
    `maker_fee`     DECIMAL(8,6) NOT NULL,
    `deal_stock`    DECIMAL(35,16) NOT NULL,
    `deal_money`    DECIMAL(35,16) NOT NULL,
    `deal_fee`      DECIMAL(35,16) NOT NULL,
    INDEX `idx_user_market` (`user_id`, `market`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- split by id, aka orer_id
CREATE TABLE `order_detail_example` (
    `id`            BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `create_time`   DOUBLE NOT NULL,
    `finish_time`   DOUBLE NOT NULL,
    `user_id`       INT UNSIGNED NOT NULL,
    `market`        VARCHAR(30) NOT NULL,
    `source`        VARCHAR(30) NOT NULL,
    `t`             TINYINT UNSIGNED NOT NULL, -- 1 limit, 2 market, 3 trigger
    `side`          TINYINT UNSIGNED NOT NULL, -- 1 卖, 2 买
    `oper_type`     TINYINT UNSIGNED NOT NULL, -- 1 open, 2 close
    `price`         DECIMAL(35,16) NOT NULL,
    `amount`        DECIMAL(35,16) NOT NULL,
    `leverage`      DECIMAL(8,2) NOT NULL,
    `trigger`       DECIMAL(35,16) NOT NULL, -- trigger price
    `taker_fee`     DECIMAL(8,6) NOT NULL,
    `maker_fee`     DECIMAL(8,6) NOT NULL,
    `deal_stock`    DECIMAL(35,16) NOT NULL,
    `deal_money`    DECIMAL(35,16) NOT NULL,
    `deal_fee`      DECIMAL(35,16) NOT NULL,
    INDEX `idx_user_market` (`user_id`, `market`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- split by order_id
CREATE TABLE `deal_history_example` (
    `id`            BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
    `time`          DOUBLE NOT NULL,
    `user_id`       INT UNSIGNED NOT NULL,
    `deal_id`       BIGINT UNSIGNED NOT NULL,
    `order_id`      BIGINT UNSIGNED NOT NULL,
    `deal_order_id` BIGINT UNSIGNED NOT NULL,
    `role`          TINYINT UNSIGNED NOT NULL,
    `price`         DECIMAL(35,16) NOT NULL,
    `amount`        DECIMAL(35,16) NOT NULL,
    `deal`          DECIMAL(35,16) NOT NULL,
    `fee`           DECIMAL(35,16) NOT NULL,
    `deal_fee`      DECIMAL(35,16) NOT NULL,
    INDEX `idx_order_id` (`order_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- split by user_id
CREATE TABLE `user_deal_history_example` (
    `id`            BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
    `time`          DOUBLE NOT NULL,
    `user_id`       INT UNSIGNED NOT NULL,
    `market`        VARCHAR(30) NOT NULL,
    `deal_id`       BIGINT UNSIGNED NOT NULL,
    `order_id`      BIGINT UNSIGNED NOT NULL,
    `deal_order_id` BIGINT UNSIGNED NOT NULL,
    `side`          TINYINT UNSIGNED NOT NULL,
    `oper_type`     TINYINT UNSIGNED NOT NULL, -- 1 open, 2 close
    `role`          TINYINT UNSIGNED NOT NULL,
    `price`         DECIMAL(35,16) NOT NULL,
    `amount`        DECIMAL(35,16) NOT NULL,
    `deal`          DECIMAL(35,16) NOT NULL,
    `fee`           DECIMAL(35,16) NOT NULL,
    `deal_fee`      DECIMAL(35,16) NOT NULL,
    `pnl`           DECIMAL(35,16) NOT NULL,
    INDEX `idx_user_market` (`user_id`, `market`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
