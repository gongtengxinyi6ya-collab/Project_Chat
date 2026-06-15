CREATE TABLE IF NOT EXISTS message_receipts (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    msg_id BIGINT UNSIGNED NOT NULL,
    account_id VARCHAR(64) NOT NULL,
    delivered_at_ms BIGINT NOT NULL DEFAULT 0,
    read_at_ms BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (id),
    UNIQUE KEY uk_msg_account (msg_id, account_id),
    KEY idx_account_delivered (account_id, delivered_at_ms),
    KEY idx_account_read (account_id, read_at_ms),
    KEY idx_msg_id (msg_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE chat_groups
    ADD COLUMN status TINYINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT '0=active,1=dissolved',
    ADD COLUMN dissolved_at_ms BIGINT UNSIGNED NULL,
    ADD KEY idx_chat_groups_status (status);