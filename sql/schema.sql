-- Project_Chat complete schema
-- Database: project_chat

CREATE DATABASE IF NOT EXISTS project_chat
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE project_chat;

-- 用户账号表
CREATE TABLE IF NOT EXISTS users (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id VARCHAR(32) NOT NULL,
    username VARCHAR(64) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    password_salt VARCHAR(64) NOT NULL,
    status TINYINT NOT NULL DEFAULT 0 COMMENT '0=normal',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uk_users_account_id (account_id),
    KEY idx_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 用户资料表
CREATE TABLE IF NOT EXISTS user_profiles (
    user_id BIGINT UNSIGNED NOT NULL,
    account_id VARCHAR(32) NOT NULL,
    username VARCHAR(64) NOT NULL,
    nickname VARCHAR(64) NOT NULL,
    avatar_url VARCHAR(512) NOT NULL DEFAULT '',
    signature VARCHAR(256) NOT NULL DEFAULT '',
    updated_at_ms BIGINT UNSIGNED NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (user_id),
    UNIQUE KEY uk_user_profiles_account_id (account_id),
    KEY idx_user_profiles_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 用户登录会话 / token 表
CREATE TABLE IF NOT EXISTS user_sessions (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id BIGINT UNSIGNED NOT NULL,
    account_id VARCHAR(32) NOT NULL,
    username VARCHAR(64) NOT NULL,
    token_hash CHAR(64) NOT NULL,
    expire_at_ms BIGINT UNSIGNED NOT NULL,
    created_at_ms BIGINT UNSIGNED NOT NULL,
    last_seen_at_ms BIGINT UNSIGNED NOT NULL,
    revoked TINYINT NOT NULL DEFAULT 0 COMMENT '0=valid,1=revoked',
    revoked_at_ms BIGINT UNSIGNED NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uk_user_sessions_token_hash (token_hash),
    KEY idx_user_sessions_account_id (account_id),
    KEY idx_user_sessions_user_id (user_id),
    KEY idx_user_sessions_expire (expire_at_ms)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 好友关系表
CREATE TABLE IF NOT EXISTS friend_relations (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id VARCHAR(32) NOT NULL,
    friend_account_id VARCHAR(32) NOT NULL,
    created_at_ms BIGINT UNSIGNED NOT NULL,
    status TINYINT NOT NULL DEFAULT 1 COMMENT '1=normal',

    PRIMARY KEY (id),
    UNIQUE KEY uk_friend_relation (account_id, friend_account_id),
    KEY idx_friend_relations_account (account_id),
    KEY idx_friend_relations_friend (friend_account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 好友申请表
CREATE TABLE IF NOT EXISTS friend_requests (
    request_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    requester_account_id VARCHAR(32) NOT NULL,
    receiver_account_id VARCHAR(32) NOT NULL,
    status TINYINT NOT NULL DEFAULT 0 COMMENT '0=pending,1=accepted,2=rejected',
    created_at_ms BIGINT UNSIGNED NOT NULL,
    handled_at_ms BIGINT UNSIGNED NULL,

    pending_pair_key VARCHAR(65)
        GENERATED ALWAYS AS (
            CASE
                WHEN status = 0 THEN CONCAT(
                    LEAST(requester_account_id, receiver_account_id),
                    '#',
                    GREATEST(requester_account_id, receiver_account_id)
                )
                ELSE NULL
            END
        ) STORED,

    PRIMARY KEY (request_id),
    UNIQUE KEY uk_friend_requests_pending_pair (pending_pair_key),
    KEY idx_friend_requests_receiver_status (receiver_account_id, status),
    KEY idx_friend_requests_requester_status (requester_account_id, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 群信息表
CREATE TABLE IF NOT EXISTS chat_groups (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    group_id VARCHAR(64) NOT NULL,
    group_name VARCHAR(128) NOT NULL,
    owner VARCHAR(64) NOT NULL,
    status TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=active,1=dissolved',
    dissolved_at_ms BIGINT UNSIGNED NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uk_chat_groups_group_id (group_id),
    KEY idx_chat_groups_owner (owner),
    KEY idx_chat_groups_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 群成员表
CREATE TABLE IF NOT EXISTS group_members (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    group_id VARCHAR(64) NOT NULL,
    account_id VARCHAR(32) NOT NULL,
    role TINYINT NOT NULL DEFAULT 0 COMMENT '0=member,1=admin,2=owner',
    joined_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uk_group_member (group_id, account_id),
    KEY idx_group_members_account_id (account_id),
    KEY idx_group_members_group_id (group_id),
    KEY idx_group_members_group_role (group_id, role)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 群入群申请表
CREATE TABLE IF NOT EXISTS group_join_requests (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    group_id VARCHAR(64) NOT NULL,
    applicant_account_id VARCHAR(32) NOT NULL,
    status TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=pending,1=approved,2=rejected',
    request_message VARCHAR(255) NOT NULL DEFAULT '',
    reviewer_account_id VARCHAR(32) NULL,
    created_at_ms BIGINT UNSIGNED NOT NULL,
    reviewed_at_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,

    PRIMARY KEY (id),
    UNIQUE KEY uk_group_join_applicant (group_id, applicant_account_id),
    KEY idx_group_join_pending (group_id, status, created_at_ms),
    KEY idx_group_join_applicant (applicant_account_id, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 群消息表
CREATE TABLE IF NOT EXISTS messages (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    msg_id BIGINT UNSIGNED NOT NULL,
    group_id VARCHAR(64) NOT NULL,
    sender_account_id VARCHAR(32) NOT NULL,
    sender_username VARCHAR(64) NOT NULL,
    content TEXT NOT NULL,
    server_ts_ms BIGINT UNSIGNED NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uk_messages_msg_id (msg_id),
    KEY idx_messages_group_time (group_id, server_ts_ms),
    KEY idx_messages_group_msg_id (group_id, msg_id),
    KEY idx_messages_sender (sender_account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 私聊消息表
CREATE TABLE IF NOT EXISTS direct_messages (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    msg_id BIGINT UNSIGNED NOT NULL,
    conversation_key VARCHAR(140) NOT NULL,
    sender_account_id VARCHAR(64) NOT NULL,
    receiver_account_id VARCHAR(64) NOT NULL,
    sender_username VARCHAR(64) NOT NULL,
    content TEXT NOT NULL,
    server_ts_ms BIGINT UNSIGNED NOT NULL,

    PRIMARY KEY (id),
    UNIQUE KEY uk_direct_msg_id (msg_id),
    KEY idx_direct_conversation_msg (conversation_key, msg_id),
    KEY idx_direct_receiver_msg (receiver_account_id, msg_id),
    KEY idx_direct_sender_msg (sender_account_id, msg_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 离线消息索引表
CREATE TABLE IF NOT EXISTS offline_messages (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id VARCHAR(32) NOT NULL,
    msg_id BIGINT UNSIGNED NOT NULL,
    group_id VARCHAR(64) NULL,
    peer_account_id VARCHAR(64) NULL,
    msg_type TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '1=group,2=direct',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uk_offline_user_msg (account_id, msg_id),
    UNIQUE KEY uk_offline_account_type_msg (account_id, msg_type, msg_id),
    KEY idx_offline_account_id_id (account_id, id),
    KEY idx_offline_account_id_msg (account_id, msg_id),
    KEY idx_offline_group_id (group_id),
    KEY idx_offline_peer_account_id (peer_account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 会话摘要 / 会话列表表
CREATE TABLE IF NOT EXISTS conversations (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    owner_account_id VARCHAR(32) NOT NULL,
    conversation_type TINYINT UNSIGNED NOT NULL COMMENT '1=direct,2=group',
    target_id VARCHAR(64) NOT NULL,

    last_msg_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    last_preview VARCHAR(256) NOT NULL DEFAULT '',
    last_sender_account_id VARCHAR(32) NOT NULL DEFAULT '',
    last_sender_username VARCHAR(64) NOT NULL DEFAULT '',
    last_ts_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,

    unread_count INT UNSIGNED NOT NULL DEFAULT 0,
    last_read_msg_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    last_read_at_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,

    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uk_conversation_owner_target (owner_account_id, conversation_type, target_id),
    KEY idx_conversations_owner_time (owner_account_id, last_ts_ms),
    KEY idx_conversations_owner_unread (owner_account_id, unread_count)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 消息送达 / 已读回执表
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

ALTER TABLE user_sessions
    ADD INDEX idx_user_sessions_revoked_cleanup
    (revoked, revoked_at_ms);

ALTER TABLE friend_requests
    ADD INDEX idx_friend_requests_cleanup
    (status, handled_at_ms);

ALTER TABLE group_join_requests
    ADD INDEX idx_group_join_requests_cleanup
    (status, reviewed_at_ms);

ALTER TABLE offline_messages
    ADD INDEX idx_offline_messages_cleanup
    (created_at);


--群会话头表
CREATE TABLE group_conversation_heads (
    group_id VARCHAR(64) NOT NULL,
    last_seq BIGINT UNSIGNED NOT NULL DEFAULT 0,
    last_msg_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    last_preview VARCHAR(256) NOT NULL DEFAULT '',
    last_sender_account_id VARCHAR(32) NOT NULL DEFAULT '',
    last_sender_username VARCHAR(64) NOT NULL DEFAULT '',
    last_ts_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
        ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (group_id),
    KEY idx_group_heads_last_ts (last_ts_ms)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci;

  --成员已读游标
  CREATE TABLE user_group_cursors (
    account_id VARCHAR(32) NOT NULL,
    group_id VARCHAR(64) NOT NULL,
    last_read_seq BIGINT UNSIGNED NOT NULL DEFAULT 0,
    last_read_msg_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    last_read_at_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,
    joined_seq BIGINT UNSIGNED NOT NULL DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
        ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (account_id, group_id),
    KEY idx_group_cursors_group (group_id)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci;

  ALTER TABLE messages
    ADD COLUMN group_seq BIGINT UNSIGNED NULL AFTER group_id;

UPDATE messages AS target
JOIN (
    SELECT
        id,
        ROW_NUMBER() OVER (
            PARTITION BY group_id
            ORDER BY msg_id ASC
        ) AS generated_seq
    FROM messages
) AS ranked
ON ranked.id = target.id
SET target.group_seq = ranked.generated_seq;

ALTER TABLE messages
    MODIFY group_seq BIGINT UNSIGNED NOT NULL,
    ADD UNIQUE KEY uk_messages_group_seq (group_id, group_seq);