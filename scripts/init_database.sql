-- =====================================================
-- Murim MMORPG Server - Database Initialization Script
-- PostgreSQL 17
-- =====================================================

-- Drop existing database if exists (for clean initialization)
DROP DATABASE IF EXISTS mh_game;

-- Create the database
CREATE DATABASE mh_game
    ENCODING = 'UTF8'
    LC_COLLATE = 'en_US.UTF-8'
    LC_CTYPE = 'en_US.UTF-8'
    TEMPLATE = template0;

-- Connect to the database
\c mh_game

-- =====================================================
-- Accounts and Sessions
-- =====================================================

-- Accounts table
CREATE TABLE accounts (
    account_id BIGSERIAL PRIMARY KEY,
    username VARCHAR(64) UNIQUE NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    salt VARCHAR(64) NOT NULL,
    email VARCHAR(128),
    phone VARCHAR(32),
    create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login_time TIMESTAMP,
    last_login_ip VARCHAR(45),
    status SMALLINT NOT NULL DEFAULT 0,  -- 0=normal, 1=banned
    ban_reason TEXT,
    ban_expire_time TIMESTAMP
);

-- Sessions table
CREATE TABLE sessions (
    session_id BIGSERIAL PRIMARY KEY,
    account_id BIGINT NOT NULL REFERENCES accounts(account_id) ON DELETE CASCADE,
    session_token VARCHAR(128) UNIQUE NOT NULL,
    server_id SMALLINT NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    login_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_active_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    logout_time TIMESTAMP,
    logout_reason SMALLINT DEFAULT 0
);

-- Indexes for accounts
CREATE INDEX idx_accounts_username ON accounts(username);
CREATE INDEX idx_accounts_email ON accounts(email);
CREATE INDEX idx_accounts_status ON accounts(status);

-- Indexes for sessions
CREATE INDEX idx_sessions_account_id ON sessions(account_id);
CREATE INDEX idx_sessions_token ON sessions(session_token);
CREATE INDEX idx_sessions_active ON sessions(logout_time) WHERE logout_time IS NULL;
CREATE INDEX idx_sessions_character ON sessions(character_id) WHERE character_id > 0;

-- =====================================================
-- Characters
-- =====================================================

-- Characters table
CREATE TABLE characters (
    character_id BIGSERIAL PRIMARY KEY,
    account_id BIGINT NOT NULL REFERENCES accounts(account_id) ON DELETE CASCADE,
    name VARCHAR(64) UNIQUE NOT NULL,
    job_class SMALLINT NOT NULL DEFAULT 0,
    initial_weapon SMALLINT NOT NULL DEFAULT 1,
    gender SMALLINT NOT NULL DEFAULT 0,
    level SMALLINT NOT NULL DEFAULT 1,
    experience BIGINT NOT NULL DEFAULT 0,
    money BIGINT NOT NULL DEFAULT 0,
    hp INTEGER NOT NULL DEFAULT 100,
    max_hp INTEGER NOT NULL DEFAULT 100,
    mp INTEGER NOT NULL DEFAULT 50,
    max_mp INTEGER NOT NULL DEFAULT 50,
    stamina INTEGER NOT NULL DEFAULT 100,
    max_stamina INTEGER NOT NULL DEFAULT 100,
    face_style SMALLINT NOT NULL DEFAULT 0,
    hair_style SMALLINT NOT NULL DEFAULT 0,
    hair_color INTEGER NOT NULL DEFAULT 0,
    map_id SMALLINT NOT NULL DEFAULT 1,
    x REAL NOT NULL DEFAULT 1000.0,
    y REAL NOT NULL DEFAULT 1000.0,
    z REAL NOT NULL DEFAULT 0.0,
    direction SMALLINT NOT NULL DEFAULT 0,
    create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login_time TIMESTAMP,
    total_play_time INTEGER NOT NULL DEFAULT 0,
    status SMALLINT NOT NULL DEFAULT 0,  -- 0=alive, 1=dead, 2=ghost
    death_time TIMESTAMP,
    last_revive_time TIMESTAMP,
    revive_protection_time TIMESTAMP
);

-- Character stats table (combat attributes)
CREATE TABLE character_stats (
    character_id BIGINT PRIMARY KEY REFERENCES characters(character_id) ON DELETE CASCADE,
    attack_power INTEGER NOT NULL DEFAULT 10,
    defense INTEGER NOT NULL DEFAULT 5,
    magic_attack INTEGER NOT NULL DEFAULT 10,
    magic_defense INTEGER NOT NULL DEFAULT 5,
    agility INTEGER NOT NULL DEFAULT 10,
    luck INTEGER NOT NULL DEFAULT 10,
    critical_rate INTEGER NOT NULL DEFAULT 5,
    critical_damage INTEGER NOT NULL DEFAULT 150,
    attack_speed INTEGER NOT NULL DEFAULT 100,
    move_speed INTEGER NOT NULL DEFAULT 100,
    hp_regen REAL NOT NULL DEFAULT 1.0,
    mp_regen REAL NOT NULL DEFAULT 0.5,
    stamina_regen REAL NOT NULL DEFAULT 2.0,
    accuracy INTEGER NOT NULL DEFAULT 90,
    dodge INTEGER NOT NULL DEFAULT 10,
    block INTEGER NOT NULL DEFAULT 5
);

-- Indexes for characters
CREATE INDEX idx_characters_account_id ON characters(account_id);
CREATE INDEX idx_characters_name ON characters(name);
CREATE INDEX idx_characters_map ON characters(map_id, x, y);
CREATE INDEX idx_characters_level ON characters(level DESC);

-- =====================================================
-- Items
-- =====================================================

-- Character items table
CREATE TABLE characters_item (
    item_id BIGSERIAL PRIMARY KEY,
    character_id BIGINT NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    item_template_id INTEGER NOT NULL,
    item_type SMALLINT NOT NULL,  -- 0=weapon, 1=armor, 2=consumable, 3=quest
    name VARCHAR(128) NOT NULL,
    quantity INTEGER NOT NULL DEFAULT 1,
    quality SMALLINT NOT NULL DEFAULT 0,  -- 0=common, 1=uncommon, 2=rare, 3=epic
    level SMALLINT NOT NULL DEFAULT 1,
    position SMALLINT,  -- inventory position (0-39)
    equip_slot SMALLINT DEFAULT -1,  -- -1=not equipped, 0-9=equipment slots
    is_bound BOOLEAN NOT NULL DEFAULT false,
    durability INTEGER NOT NULL DEFAULT 100,
    max_durability INTEGER NOT NULL DEFAULT 100,
    create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Item stats table (for equipment)
CREATE TABLE characters_item_stats (
    item_id BIGINT PRIMARY KEY REFERENCES characters_item(item_id) ON DELETE CASCADE,
    attack_power INTEGER DEFAULT 0,
    defense INTEGER DEFAULT 0,
    magic_attack INTEGER DEFAULT 0,
    magic_defense INTEGER DEFAULT 0,
    agility INTEGER DEFAULT 0,
    luck INTEGER DEFAULT 0,
    hp_bonus INTEGER DEFAULT 0,
    mp_bonus INTEGER DEFAULT 0
);

-- Indexes for items
CREATE INDEX idx_items_character_id ON characters_item(character_id);
CREATE INDEX idx_items_template ON characters_item(item_template_id);

-- =====================================================
-- Skills
-- =====================================================

-- Character skills table
CREATE TABLE characters_skill (
    skill_id BIGSERIAL PRIMARY KEY,
    character_id BIGINT NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    skill_template_id INTEGER NOT NULL,
    level SMALLINT NOT NULL DEFAULT 1,
    exp INTEGER NOT NULL DEFAULT 0,
    is_active BOOLEAN NOT NULL DEFAULT true
);

-- Indexes for skills
CREATE INDEX idx_skills_character_id ON characters_skill(character_id);

-- =====================================================
-- Quests
-- =====================================================

-- Quest progress table
CREATE TABLE quest_progress (
    quest_id BIGSERIAL PRIMARY KEY,
    character_id BIGINT NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    quest_template_id INTEGER NOT NULL,
    status SMALLINT NOT NULL DEFAULT 0,  -- 0=not_started, 1=in_progress, 2=completed, 3=failed
    current_stage SMALLINT NOT NULL DEFAULT 0,
    progress_data TEXT,  -- JSON-encoded progress data
    start_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    complete_time TIMESTAMP
);

-- Indexes for quests
CREATE INDEX idx_quests_character_id ON quest_progress(character_id);
CREATE INDEX idx_quests_status ON quest_progress(status);

-- =====================================================
-- Guilds
-- =====================================================

-- Guilds table
CREATE TABLE guild_info (
    guild_id BIGSERIAL PRIMARY KEY,
    guild_name VARCHAR(64) UNIQUE NOT NULL,
    leader_id BIGINT NOT NULL REFERENCES characters(character_id),
    level SMALLINT NOT NULL DEFAULT 1,
    experience INTEGER NOT NULL DEFAULT 0,
    member_count INTEGER NOT NULL DEFAULT 1,
    max_members INTEGER NOT NULL DEFAULT 50,
    notice TEXT,
    create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Guild members table
CREATE TABLE guild_members (
    guild_id BIGINT NOT NULL REFERENCES guild_info(guild_id) ON DELETE CASCADE,
    character_id BIGINT NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    position SMALLINT NOT NULL DEFAULT 0,  -- 0=member, 1=officer, 2=leader
    join_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    contribution INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (guild_id, character_id)
);

-- Indexes for guilds
CREATE INDEX idx_guilds_name ON guild_info(guild_name);
CREATE INDEX idx_guilds_leader ON guild_info(leader_id);
CREATE INDEX idx_guild_members_character ON guild_members(character_id);

-- =====================================================
-- Parties
-- =====================================================

-- Party info table
CREATE TABLE party_info (
    party_id BIGSERIAL PRIMARY KEY,
    leader_id BIGINT NOT NULL REFERENCES characters(character_id),
    member_count INTEGER NOT NULL DEFAULT 1,
    max_members INTEGER NOT NULL DEFAULT 8,
    create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Party members table
CREATE TABLE party_members (
    party_id BIGINT NOT NULL REFERENCES party_info(party_id) ON DELETE CASCADE,
    character_id BIGINT NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    join_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (party_id, character_id)
);

-- Indexes for parties
CREATE INDEX idx_party_leader ON party_info(leader_id);
CREATE INDEX idx_party_members_character ON party_members(character_id);

-- =====================================================
-- Friends
-- =====================================================

-- Friend relationships table
CREATE TABLE friend_relationships (
    relationship_id BIGSERIAL PRIMARY KEY,
    character_id BIGINT NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    friend_id BIGINT NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    status SMALLINT NOT NULL DEFAULT 0,  -- 0=pending, 1=accepted, 2=blocked
    create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(character_id, friend_id)
);

-- Indexes for friends
CREATE INDEX idx_friends_character ON friend_relationships(character_id);
CREATE INDEX idx_friends_friend ON friend_relationships(friend_id);

-- =====================================================
-- Mail
-- =====================================================

-- Mail table
CREATE TABLE mail (
    mail_id BIGSERIAL PRIMARY KEY,
    sender_id BIGINT REFERENCES characters(character_id) ON DELETE SET NULL,
    receiver_id BIGINT NOT NULL REFERENCES characters(character_id) ON DELETE CASCADE,
    subject VARCHAR(256) NOT NULL,
    body TEXT NOT NULL,
    money BIGINT NOT NULL DEFAULT 0,
    item_id BIGINT REFERENCES characters_item(item_id) ON DELETE SET NULL,
    is_read BOOLEAN NOT NULL DEFAULT false,
    is_claimed BOOLEAN NOT NULL DEFAULT false,
    send_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expire_time TIMESTAMP NOT NULL DEFAULT (CURRENT_TIMESTAMP + INTERVAL '30 days')
);

-- Indexes for mail
CREATE INDEX idx_mail_receiver ON mail(receiver_id);
CREATE INDEX idx_mail_read ON mail(is_read) WHERE is_read = false;

-- =====================================================
-- Chat
-- =====================================================

-- Chat messages table (for logging/history)
CREATE TABLE chat_messages (
    message_id BIGSERIAL PRIMARY KEY,
    channel_type SMALLINT NOT NULL,  -- 0=world, 1=shout, 2=guild, 3=party, 4=whisper, 5=system
    sender_id BIGINT REFERENCES characters(character_id) ON DELETE SET NULL,
    receiver_id BIGINT REFERENCES characters(character_id) ON DELETE SET NULL,
    message TEXT NOT NULL,
    send_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for chat
CREATE INDEX idx_chat_time ON chat_messages(send_time DESC);
CREATE INDEX idx_chat_sender ON chat_messages(sender_id);
CREATE INDEX idx_chat_channel ON chat_messages(channel_type);

-- =====================================================
-- Insert test data
-- =====================================================

-- Insert test account (username: test, password: test123)
-- Password hash is SHA256 of "test123salt_test_account"
INSERT INTO accounts (username, password_hash, salt, email) VALUES
('test', 'a94f9a3e5b8b5c9d8f7e6a5b4c3d2e1f9a8b7c6d5e4f3a2b1c0d9e8f7a6b5c4d', 'salt_test_account', 'test@example.com');

-- Insert test characters for the test account
INSERT INTO characters (account_id, name, job_class, initial_weapon, gender, level) VALUES
(1, 'TestWarrior', 1, 1, 0, 10),
(1, 'TestMage', 2, 2, 1, 8),
(1, 'TestArcher', 3, 3, 0, 5);

-- Insert character stats
INSERT INTO character_stats (character_id, attack_power, defense, agility, luck) VALUES
(1, 50, 30, 20, 15),
(2, 60, 20, 25, 20),
(3, 70, 15, 35, 18);

-- =====================================================
-- Grant permissions
-- =====================================================

-- Grant necessary permissions to postgres user
GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO postgres;
GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO postgres;

-- =====================================================
-- Verification queries
-- =====================================================

-- Display database info
SELECT 'Database initialization completed!' AS status;
SELECT 'Tables created:' AS info;
SELECT tablename FROM pg_tables WHERE schemaname = 'public' ORDER BY tablename;

-- Display test data
SELECT 'Test accounts created:' AS info;
SELECT account_id, username, email, status FROM accounts;

SELECT 'Test characters created:' AS info;
SELECT character_id, account_id, name, level, job_class FROM characters;
