-- ============================================================
--  mod-warden-chat | SQL Setup
--  Ejecutar en: acore_auth
-- ============================================================

USE acore_auth;

-- ------------------------------------------------------------
-- Palabras prohibidas
--   sanction:    0 = solo registrar      | 1 = mute automático
--   block:       0 = mensaje se envía    | 1 = mensaje bloqueado
--   replacement: NULL = usar global conf | texto = reemplazar por esta frase
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `warden_blacklist` (
    `id`          INT UNSIGNED  NOT NULL AUTO_INCREMENT,
    `word`        VARCHAR(128)  NOT NULL,
    `sanction`    TINYINT(1)    NOT NULL DEFAULT 0,
    `block`       TINYINT(1)    NOT NULL DEFAULT 0,
    `replacement` VARCHAR(255)  NULL     DEFAULT NULL,
    `added_by`    VARCHAR(64)   NOT NULL DEFAULT 'system',
    `added_at`    DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_word` (`word`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ------------------------------------------------------------
-- Mensajes flagueados
--   sender_team: 0 = Alliance | 1 = Horde | 2 = Neutral
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `warden_flagged_messages` (
    `id`               BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `sender_guid`      INT UNSIGNED    NOT NULL,
    `sender_name`      VARCHAR(64)     NOT NULL,
    `sender_class`     TINYINT         NOT NULL DEFAULT 0,
    `sender_team`      TINYINT         NOT NULL DEFAULT 0,
    `channel_label`    VARCHAR(32)     NOT NULL,
    `channel_name`     VARCHAR(64)     NOT NULL DEFAULT '' COMMENT 'Nombre del canal (solo para CHANNEL)',
    `triggered_word`   VARCHAR(128)    NOT NULL,
    `sanction_applied` TINYINT(1)      NOT NULL DEFAULT 0,
    `message`          TEXT            NOT NULL,
    `sent_at`          DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_sender`  (`sender_guid`),
    KEY `idx_sent_at` (`sent_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ------------------------------------------------------------
-- Log completo de susurros
--   whisper_retention: configurable en conf (0 = nunca borrar)
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `warden_whisper_log` (
    `id`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `sender_guid`    INT UNSIGNED    NOT NULL,
    `sender_name`    VARCHAR(64)     NOT NULL,
    `sender_class`   TINYINT         NOT NULL DEFAULT 0,
    `sender_team`    TINYINT         NOT NULL DEFAULT 0,
    `receiver_guid`  INT UNSIGNED    NOT NULL,
    `receiver_name`  VARCHAR(64)     NOT NULL,
    `receiver_class` TINYINT         NOT NULL DEFAULT 0,
    `receiver_team`  TINYINT         NOT NULL DEFAULT 0,
    `message`        TEXT            NOT NULL,
    `sent_at`        DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_sender`   (`sender_guid`),
    KEY `idx_receiver` (`receiver_guid`),
    KEY `idx_sent_at`  (`sent_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ------------------------------------------------------------
-- Palabras de ejemplo
-- ------------------------------------------------------------
INSERT IGNORE INTO `warden_blacklist` (`word`, `sanction`, `block`, `replacement`, `added_by`) VALUES
    ('nigger',    0, 0, NULL, 'system');
