
ALTER TABLE chat_groups
    ADD COLUMN status TINYINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT '0=active,1=dissolved',
    ADD COLUMN dissolved_at_ms BIGINT UNSIGNED NULL,
    ADD KEY idx_chat_groups_status (status);



ALTER TABLE messages
    ADD KEY idx_messages_group_msg_id (group_id, msg_id);


INSERT INTO group_conversation_heads (
    group_id,
    last_seq,
    last_msg_id,
    last_preview,
    last_sender_account_id,
    last_sender_username,
    last_ts_ms
)
SELECT
    group_id,
    0,
    0,
    '',
    '',
    '',
    0
FROM chat_groups
WHERE status = 0
ON DUPLICATE KEY UPDATE
    group_id = VALUES(group_id);

    INSERT INTO group_conversation_heads (
    group_id,
    last_seq,
    last_msg_id,
    last_preview,
    last_sender_account_id,
    last_sender_username,
    last_ts_ms
)
SELECT
    latest.group_id,
    latest.group_seq,
    latest.msg_id,
    LEFT(latest.content, 200),
    latest.sender_account_id,
    latest.sender_username,
    latest.server_ts_ms
FROM (
    SELECT
        messages.*,
        ROW_NUMBER() OVER (
            PARTITION BY group_id
            ORDER BY group_seq DESC
        ) AS row_number_in_group
    FROM messages
) AS latest
WHERE latest.row_number_in_group = 1
ON DUPLICATE KEY UPDATE
    last_seq = VALUES(last_seq),
    last_msg_id = VALUES(last_msg_id),
    last_preview = VALUES(last_preview),
    last_sender_account_id =
        VALUES(last_sender_account_id),
    last_sender_username =
        VALUES(last_sender_username),
    last_ts_ms = VALUES(last_ts_ms);

    INSERT INTO user_group_cursors (
    account_id,
    group_id,
    last_read_seq,
    last_read_msg_id,
    last_read_at_ms,
    joined_seq
)
SELECT
    member.account_id,
    member.group_id,
    COALESCE(head.last_seq, 0),
    COALESCE(head.last_msg_id, 0),
    0,
    COALESCE(head.last_seq, 0)
FROM group_members AS member
LEFT JOIN group_conversation_heads AS head
    ON head.group_id = member.group_id
ON DUPLICATE KEY UPDATE
    account_id = VALUES(account_id);