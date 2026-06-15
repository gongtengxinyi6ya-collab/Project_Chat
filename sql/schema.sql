CREATE DATABASE IF NOT EXISTS project_chat
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

use project_chat;
CREATE TABLE IF NOT EXISTS users (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id VARCHAR(32) NOT NULL,
    username VARCHAR(64) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    password_hash VARCHAR(255) NOT NULL,
    password_salt VARCHAR(64) NOT NULL,
    status TINYINT NOT NULL DEFAULT 0,
    PRIMARY KEY (id),
    UNIQUE KEY uk_users_account_id (account_id),
    KEY idx_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS chat_groups(
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    group_id VARCHAR(64) NOT NULL,
    group_name VARCHAR(128) NOT NULL,
    owner VARCHAR(64) NOT NULL,
    status TINYINT UNSIGNED NOT NULL DEFAULT 0,
    dissolved_at_ms BIGINT UNSIGNED NULL,
    KEY idx_chat_groups_status (status),
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_chat_groups_group_id (group_id),
    KEY idx_chat_groups_owner (owner)
)ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS group_members (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    group_id VARCHAR(64) NOT NULL,
    account_id VARCHAR(32) NOT NULL,
    role TINYINT NOT NULL DEFAULT 0,
    joined_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_group_member (group_id, account_id),
    KEY idx_group_members_account_id (account_id),
    KEY idx_group_members_group_id (group_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
ALTER TABLE group_members
MODIFY COLUMN role TINYINT NOT NULL DEFAULT 0 COMMENT '0=member,1=admin,2=owner';
CREATE INDEX idx_group_members_group_role
ON group_members(group_id, role);

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
    KEY idx_messages_sender (sender_account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS offline_messages(
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id VARCHAR(32) NOT NULL,
    msg_id BIGINT UNSIGNED NOT NULL,
    group_id VARCHAR(64) NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_offline_user_msg (account_id, msg_id),
    KEY idx_offline_account_id_id (account_id, id),
    KEY idx_offline_account_id_msg (account_id, msg_id),
    KEY idx_offline_group_id (group_id)
)ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
ALTER TABLE offline_messages
ADD COLUMN msg_type TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '1=group,2=direct';

ALTER TABLE offline_messages
ADD COLUMN peer_account_id VARCHAR(64) NULL AFTER group_id;

ALTER TABLE offline_messages
ADD UNIQUE KEY uk_offline_account_type_msg (
    account_id,
    msg_type,
    msg_id
);
CREATE TABLE IF NOT EXISTS user_sessions (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id BIGINT UNSIGNED NOT NULL,
    account_id VARCHAR(32) NOT NULL,
    username VARCHAR(64) NOT NULL,
    token_hash CHAR(64) NOT NULL,
    expire_at_ms BIGINT UNSIGNED NOT NULL,
    created_at_ms BIGINT UNSIGNED NOT NULL,
    last_seen_at_ms BIGINT UNSIGNED NOT NULL,
    revoked TINYINT NOT NULL DEFAULT 0,
    revoked_at_ms BIGINT UNSIGNED NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_sessions_token_hash (token_hash),
    KEY idx_user_sessions_account_id (account_id),
    KEY idx_user_sessions_user_id (user_id),
    KEY idx_user_sessions_expire (expire_at_ms)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS friend_relations (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id VARCHAR(32) NOT NULL,
    friend_account_id VARCHAR(32) NOT NULL,
    created_at_ms BIGINT UNSIGNED NOT NULL,
    status TINYINT NOT NULL DEFAULT 1,
    PRIMARY KEY (id),
    UNIQUE KEY uk_friend_relation (account_id, friend_account_id),
    KEY idx_friend_relations_account (account_id),
    KEY idx_friend_relations_friend (friend_account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS friend_requests (
    request_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    requester_account_id VARCHAR(32) NOT NULL,
    receiver_account_id VARCHAR(32) NOT NULL,
    status TINYINT NOT NULL DEFAULT 0,
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

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
    KEY idx_direct_conversation_msg (
        conversation_key,
        msg_id
    ),
    KEY idx_direct_receiver_msg (
        receiver_account_id,
        msg_id
    ),
    KEY idx_direct_sender_msg (
        sender_account_id,
        msg_id
    )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


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
    UNIQUE KEY uk_conversation_owner_target (
        owner_account_id,
        conversation_type,
        target_id
    ),
    KEY idx_conversations_owner_time (
        owner_account_id,
        last_ts_ms
    ),
    KEY idx_conversations_owner_unread (
        owner_account_id,
        unread_count
    )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

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