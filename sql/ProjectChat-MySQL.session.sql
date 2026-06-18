
ALTER TABLE chat_groups
    ADD COLUMN status TINYINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT '0=active,1=dissolved',
    ADD COLUMN dissolved_at_ms BIGINT UNSIGNED NULL,
    ADD KEY idx_chat_groups_status (status);



ALTER TABLE messages
    ADD KEY idx_messages_group_msg_id (group_id, msg_id);