CREATE DATABASE IF NOT EXISTS project_chat
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

use project_chat;

CREATE TABLE IF NOT EXISTS users (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    password_hash VARCHAR(255) NOT NULL,
    password_salt VARCHAR(64) NOT NULL,
    status TINYINT NOT NULL DEFAULT 0,
    PRIMARY KEY (id),
    UNIQUE KEY uk_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


CREATE TABLE IF NOT EXISTS chat_groups(
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    group_id VARCHAR(64) NOT NULL,
    group_name VARCHAR(128) NOT NULL,
    owner VARCHAR(64) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_chat_groups_group_id (group_id),
    KEY idx_chat_groups_owner (owner)
)ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS group_members (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    group_id VARCHAR(64) NOT NULL,
    username VARCHAR(64) NOT NULL,
    role TINYINT NOT NULL DEFAULT 0,
    joined_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_group_member (group_id, username),
    KEY idx_group_members_username (username),
    KEY idx_group_members_group_id (group_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS messages (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    msg_id BIGINT UNSIGNED NOT NULL,
    group_id VARCHAR(64) NOT NULL,
    sender VARCHAR(64) NOT NULL,
    content TEXT NOT NULL,
    server_ts_ms BIGINT UNSIGNED NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_messages_msg_id (msg_id),
    KEY idx_messages_group_time (group_id, server_ts_ms),
    KEY idx_messages_sender (sender)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS offline_messages(
    id BIGINT UNSIGNED NOT NULL AUTO_INCREAMENT,
    username VARCHAR(64) NOT NULL,
    msg_id BIGINT UNSIGNED NOT NULL,
    group_id VARCHAR(64) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_offline_user_msg (username, msg_id),
    KEY idx_offline_username_id (username, id),
    KEY idx_offline_username_msg (username, msg_id),
    KEY idx_offline_group_id (group_id)
)ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;