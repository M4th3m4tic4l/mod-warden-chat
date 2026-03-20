#include "ScriptMgr.h"
#include "PlayerScript.h"
#include "CommandScript.h"
#include "Player.h"
#include "Channel.h"
#include "Group.h"
#include "Guild.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <shared_mutex>
#include <ctime>

using namespace Acore::ChatCommands;

// ============================================================
//  Colores
// ============================================================
static const char* WC_ChannelColor(uint32 type)
{
    switch (type)
    {
        case CHAT_MSG_SAY:                  return "|cFFFFFFFF"; // Blanco
        case CHAT_MSG_YELL:                 return "|cFFFF3F40"; // Rojo
        case CHAT_MSG_EMOTE:                return "|cFFFF7E40"; // Naranja suave
        case CHAT_MSG_WHISPER:              return "|cFFFF7EFF"; // Rosa/violeta whisper
        case CHAT_MSG_PARTY:                return "|cFFAAABFE"; // Azul lavanda party
        case CHAT_MSG_PARTY_LEADER:         return "|cFF77C8FF"; // Azul claro party leader
        case CHAT_MSG_RAID:                 return "|cFFFF7F00"; // Naranja raid
        case CHAT_MSG_RAID_LEADER:          return "|cFFFF7F00"; // Naranja raid
        case CHAT_MSG_RAID_WARNING:         return "|cFFFF4700"; // Rojo-naranja warning
        case CHAT_MSG_BATTLEGROUND:         return "|cFFFF7D01"; // Naranja BG
        case CHAT_MSG_BATTLEGROUND_LEADER:  return "|cFFFF4709"; // Naranja oscuro BG leader
        case CHAT_MSG_GUILD:                return "|cFF3CE13F"; // Verde hermandad
        case CHAT_MSG_OFFICER:              return "|cFF40BC40"; // Verde oscuro oficiales
        case CHAT_MSG_CHANNEL:              return "|cFFFEC1C0"; // Salmón/piel canal general
        default:                            return "|cFFCCCCCC"; // Gris
    }
}

static const char* WC_ClassColor(uint8 classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:      return "|cFFC79C6E";
        case CLASS_PALADIN:      return "|cFFF58CBA";
        case CLASS_HUNTER:       return "|cFFABD473";
        case CLASS_ROGUE:        return "|cFFFFF569";
        case CLASS_PRIEST:       return "|cFFFFFFFF";
        case CLASS_DEATH_KNIGHT: return "|cFFC41F3B";
        case CLASS_SHAMAN:       return "|cFF0070DE";
        case CLASS_MAGE:         return "|cFF40C7EB";
        case CLASS_WARLOCK:      return "|cFF8787ED";
        case CLASS_DRUID:        return "|cFFFF7D0A";
        default:                 return "|cFFFFFFFF";
    }
}

static std::string WC_ClassIcon(uint8 classId)
{
    // Iconos de clase del cliente WoW 3.3.5a
    switch (classId)
    {
        case CLASS_WARRIOR:      return "|TInterface\\Icons\\INV_Sword_27:14:14|t";
        case CLASS_PALADIN:      return "|TInterface\\Icons\\Spell_Holy_HolyBolt:14:14|t";
        case CLASS_HUNTER:       return "|TInterface\\Icons\\INV_Weapon_Bow_07:14:14|t";
        case CLASS_ROGUE:        return "|TInterface\\Icons\\INV_ThrowingKnife_04:14:14|t";
        case CLASS_PRIEST:       return "|TInterface\\Icons\\INV_Staff_30:14:14|t";
        case CLASS_DEATH_KNIGHT: return "|TInterface\\Icons\\Spell_DeathKnight_ClassIcon:14:14|t";
        case CLASS_SHAMAN:       return "|TInterface\\Icons\\Spell_Nature_BloodLust:14:14|t";
        case CLASS_MAGE:         return "|TInterface\\Icons\\INV_Staff_13:14:14|t";
        case CLASS_WARLOCK:      return "|TInterface\\Icons\\Spell_Nature_FaerieFire:14:14|t";
        case CLASS_DRUID:        return "|TInterface\\Icons\\Ability_Druid_Maul:14:14|t";
        default:                 return "";
    }
}

static std::string WC_FactionIcon(uint8 team)
{
    switch (team)
    {
        case TEAM_ALLIANCE: return "|TInterface\\PVPFrame\\PVP-Currency-Alliance:14:14|t";
        case TEAM_HORDE:    return "|TInterface\\PVPFrame\\PVP-Currency-Horde:14:14|t";
        default:            return "";
    }
}

static const char* WC_ChannelLabel(uint32 type)
{
    switch (type)
    {
        case CHAT_MSG_SAY:                  return "SAY";
        case CHAT_MSG_YELL:                 return "YELL";
        case CHAT_MSG_EMOTE:                return "EMOTE";
        case CHAT_MSG_WHISPER:              return "WHISPER";
        case CHAT_MSG_PARTY:                return "PARTY";
        case CHAT_MSG_PARTY_LEADER:         return "PARTY_LEADER";
        case CHAT_MSG_RAID:                 return "RAID";
        case CHAT_MSG_RAID_LEADER:          return "RAID_LEADER";
        case CHAT_MSG_RAID_WARNING:         return "RAID_WARNING";
        case CHAT_MSG_BATTLEGROUND:         return "BATTLEGROUND";
        case CHAT_MSG_BATTLEGROUND_LEADER:  return "BATTLEGROUND_LEADER";
        case CHAT_MSG_GUILD:                return "GUILD";
        case CHAT_MSG_OFFICER:              return "OFFICER";
        case CHAT_MSG_CHANNEL:              return "CHANNEL";
        default:                            return "OTHER";
    }
}

static const char* ChannelLabelToColor(const std::string& label)
{
    if (label == "SAY")                                             return "|cFFFFFFFF";
    if (label == "YELL")                                            return "|cFFFF3F40";
    if (label == "EMOTE")                                           return "|cFFFF7E40";
    if (label == "WHISPER")                                         return "|cFFFF7EFF";
    if (label == "PARTY"        || label == "PARTY_LEADER")         return "|cFFAAABFE";
    if (label == "RAID"         || label == "RAID_LEADER")          return "|cFFFF7F00";
    if (label == "RAID_WARNING")                                    return "|cFFFF4500";
    if (label == "BATTLEGROUND" || label == "BATTLEGROUND_LEADER")  return "|cFFFF7D01";
    if (label == "GUILD")                                           return "|cFF3CE13F";
    if (label == "OFFICER")                                         return "|cFF40BC40";
    if (label == "CHANNEL")                                         return "|cFFFEC1C0";
    return "|cFFCCCCCC";
}

static std::string WC_ToLower(const std::string& s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return r;
}

static std::string WC_ToUpper(const std::string& s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return std::toupper(c); });
    return r;
}

// ============================================================
//  Blacklist word struct
// ============================================================
struct BlacklistWord
{
    std::string word;
    bool        sanction;
    bool        block;
    std::string replacement;
};

// ============================================================
//  Singleton
// ============================================================
class WardenChat
{
public:
    static WardenChat* instance()
    {
        static WardenChat _inst;
        return &_inst;
    }

    bool        enabled        = true;
    uint8       minGMLevel     = 2;
    bool        muteEnabled    = false;
    uint32      muteDuration   = 60;
    uint32      retentionDays  = 90;
    bool        blockEnabled      = false;
    bool        replaceEnabled    = false;
    std::string replaceText       = "";
    bool        whisperLogEnabled  = true;
    uint32      whisperRetention   = 90;   // días, 0 = nunca borrar
    bool        watchCanSpyGMs     = false; // WardenChat.Watch.AllowSpyOnGMs
    uint8       watchMinGMToSpy    = 0;     // Nivel mínimo de GM que puede chuzar a otros GMs

    void LoadConfig()
    {
        enabled        = sConfigMgr->GetOption<bool>       ("WardenChat.Enable",               true);
        minGMLevel     = sConfigMgr->GetOption<uint8>      ("WardenChat.MinGMLevel",           2);
        muteEnabled    = sConfigMgr->GetOption<bool>       ("WardenChat.Mute.Enable",          false);
        muteDuration   = sConfigMgr->GetOption<uint32>     ("WardenChat.Mute.DurationSeconds", 60);
        retentionDays  = sConfigMgr->GetOption<uint32>     ("WardenChat.Retention.Days",       90);
        blockEnabled   = sConfigMgr->GetOption<bool>       ("WardenChat.Block.Enable",         false);
        replaceEnabled = sConfigMgr->GetOption<bool>       ("WardenChat.Replace.Enable",       false);
        replaceText       = sConfigMgr->GetOption<std::string>("WardenChat.Replace.Text",          "");
        whisperLogEnabled = sConfigMgr->GetOption<bool>       ("WardenChat.WhisperLog.Enable",   true);
        whisperRetention  = sConfigMgr->GetOption<uint32>("WardenChat.WhisperLog.RetentionDays",  90);
        watchCanSpyGMs    = sConfigMgr->GetOption<bool> ("WardenChat.Watch.AllowSpyOnGMs",      false);
        watchMinGMToSpy   = sConfigMgr->GetOption<uint8>("WardenChat.Watch.MinLevelToSpyOnGMs", 3);
    }

    void LoadBlacklist()
    {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        _words.clear();

        QueryResult result = LoginDatabase.Query(
            "SELECT word, sanction, block, COALESCE(replacement, '') "
            "FROM warden_blacklist");

        if (!result)
        {
            LOG_INFO("module", "mod-warden-chat: Blacklist vacía.");
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            BlacklistWord bw;
            bw.word        = WC_ToLower(fields[0].Get<std::string>());
            bw.sanction    = fields[1].Get<bool>();
            bw.block       = fields[2].Get<bool>();
            bw.replacement = fields[3].Get<std::string>();
            _words.push_back(bw);
        } while (result->NextRow());

        LOG_INFO("module", "mod-warden-chat: {} palabras cargadas.", _words.size());
    }

    void CleanupExpired()
    {
        if (retentionDays == 0)
            return;

        LoginDatabase.Execute(
            "DELETE FROM warden_flagged_messages "
            "WHERE sent_at < NOW() - INTERVAL {} DAY",
            retentionDays);
    }

    void CleanupWhispers()
    {
        if (whisperRetention == 0)
            return;

        LoginDatabase.Execute(
            "DELETE FROM warden_whisper_log "
            "WHERE sent_at < NOW() - INTERVAL {} DAY",
            whisperRetention);
    }

    const BlacklistWord* Check(const std::string& msg) const
    {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        std::string lower = WC_ToLower(msg);
        for (const auto& bw : _words)
            if (lower.find(bw.word) != std::string::npos)
                return &bw;
        return nullptr;
    }

    void SaveFlagged(
        uint32             senderGuid,
        const std::string& senderName,
        uint8              senderClass,
        uint8              senderTeam,
        const std::string& channelLabel,
        const std::string& channelName,
        const std::string& triggeredWord,
        bool               sanctionApplied,
        const std::string& message)
    {
        std::string eName  = senderName;
        std::string eChan  = channelName;
        std::string eWord  = triggeredWord;
        std::string eMsg   = message;

        LoginDatabase.EscapeString(eName);
        LoginDatabase.EscapeString(eChan);
        LoginDatabase.EscapeString(eWord);
        LoginDatabase.EscapeString(eMsg);

        LoginDatabase.Execute(
            "INSERT INTO warden_flagged_messages "
            "(sender_guid, sender_name, sender_class, sender_team, "
            " channel_label, channel_name, triggered_word, sanction_applied, message) "
            "VALUES ({}, '{}', {}, {}, '{}', '{}', '{}', {}, '{}')",
            senderGuid, eName, senderClass, senderTeam,
            channelLabel, eChan, eWord, sanctionApplied ? 1 : 0, eMsg);
    }

    // Watchlist
    void WatchAdd(uint32 gmGuid, uint32 targetGuid)
    {
        std::unique_lock<std::shared_mutex> lock(_watchMutex);
        _watchMap[targetGuid].insert(gmGuid);
    }

    void WatchRemove(uint32 gmGuid, uint32 targetGuid)
    {
        std::unique_lock<std::shared_mutex> lock(_watchMutex);
        auto it = _watchMap.find(targetGuid);
        if (it != _watchMap.end())
        {
            it->second.erase(gmGuid);
            if (it->second.empty())
                _watchMap.erase(it);
        }
    }

    void WatchRemoveGM(uint32 gmGuid)
    {
        std::unique_lock<std::shared_mutex> lock(_watchMutex);
        for (auto it = _watchMap.begin(); it != _watchMap.end(); )
        {
            it->second.erase(gmGuid);
            it->second.empty() ? it = _watchMap.erase(it) : ++it;
        }
    }

    std::set<uint32> WatchGetGMs(uint32 targetGuid) const
    {
        std::shared_lock<std::shared_mutex> lock(_watchMutex);
        auto it = _watchMap.find(targetGuid);
        return it != _watchMap.end() ? it->second : std::set<uint32>{};
    }

    std::vector<uint32> WatchGetTargets(uint32 gmGuid) const
    {
        std::shared_lock<std::shared_mutex> lock(_watchMutex);
        std::vector<uint32> targets;
        for (const auto& [targetGuid, gms] : _watchMap)
            if (gms.count(gmGuid))
                targets.push_back(targetGuid);
        return targets;
    }

    void SaveWhisper(
        uint32             senderGuid,
        const std::string& senderName,
        uint8              senderClass,
        uint8              senderTeam,
        uint32             receiverGuid,
        const std::string& receiverName,
        uint8              receiverClass,
        uint8              receiverTeam,
        const std::string& message)
    {
        std::string eSender   = senderName;
        std::string eReceiver = receiverName;
        std::string eMsg      = message;

        LoginDatabase.EscapeString(eSender);
        LoginDatabase.EscapeString(eReceiver);
        LoginDatabase.EscapeString(eMsg);

        LoginDatabase.Execute(
            "INSERT INTO warden_whisper_log "
            "(sender_guid, sender_name, sender_class, sender_team, "
            " receiver_guid, receiver_name, receiver_class, receiver_team, message) "
            "VALUES ({}, '{}', {}, {}, {}, '{}', {}, {}, '{}')",
            senderGuid, eSender, senderClass, senderTeam,
            receiverGuid, eReceiver, receiverClass, receiverTeam, eMsg);
    }

        // WatchGroup — gmGuid → guid del jugador ancla
    void WatchGroupAdd(uint32 gmGuid, uint32 anchorGuid)
    {
        std::unique_lock<std::shared_mutex> lock(_wgMutex);
        _watchGroupMap[gmGuid] = anchorGuid;
    }

    void WatchGroupRemove(uint32 gmGuid)
    {
        std::unique_lock<std::shared_mutex> lock(_wgMutex);
        _watchGroupMap.erase(gmGuid);
    }

    void WatchGroupRemoveAll(uint32 gmGuid)
    {
        WatchGroupRemove(gmGuid);
    }

    // Devuelve todos los GMs que tienen watchgroup activo y cuyo ancla
    // pertenece al mismo grupo que groupGuid
    // Llamado desde Handle() con el guid del emisor del mensaje
    std::vector<uint32> WatchGroupGetGMs(uint32 senderGuid) const
    {
        std::shared_lock<std::shared_mutex> lock(_wgMutex);
        std::vector<uint32> result;
        for (const auto& [gmGuid, anchorGuid] : _watchGroupMap)
        {
            // El emisor mismo es el ancla, o comparten grupo — se verifica en Handle()
            if (anchorGuid == senderGuid)
                result.push_back(gmGuid);
        }
        return result;
    }

    // Devuelve el guid ancla de un GM, o 0 si no tiene watchgroup
    uint32 WatchGroupGetAnchor(uint32 gmGuid) const
    {
        std::shared_lock<std::shared_mutex> lock(_wgMutex);
        auto it = _watchGroupMap.find(gmGuid);
        return it != _watchGroupMap.end() ? it->second : 0;
    }

    // Devuelve todos los pares {targetGuid, gmGuid} del watchmap normal
    std::unordered_map<uint32, std::set<uint32>> WatchGetAllPairs() const
    {
        std::shared_lock<std::shared_mutex> lock(_watchMutex);
        return _watchMap;
    }

    // Devuelve todos los pares gmGuid->anchorGuid activos
    std::unordered_map<uint32, uint32> WatchGroupGetAll() const
    {
        std::shared_lock<std::shared_mutex> lock(_wgMutex);
        return _watchGroupMap;
    }

private:
    WardenChat() = default;
    mutable std::shared_mutex  _mutex;
    std::vector<BlacklistWord> _words;
    mutable std::shared_mutex                    _watchMutex;
    std::unordered_map<uint32, std::set<uint32>> _watchMap;
    mutable std::shared_mutex                    _wgMutex;
    std::unordered_map<uint32, uint32>           _watchGroupMap; // gmGuid → anchorGuid
};

#define sWardenChat WardenChat::instance()

// ============================================================
//  Display helpers
// ============================================================
// Campos del SELECT en blacklist queries:
//   f[0]=sent_at     f[1]=sender_name   f[2]=sender_class
//   f[3]=sender_team f[4]=channel_label f[5]=channel_name
//   f[6]=triggered_word                 f[7]=message (LEFT 60)

static std::string BuildRowByDate(Field* f)
{
    std::string sentAt   = f[0].Get<std::string>();
    std::string name     = f[1].Get<std::string>();
    uint8 cls            = f[2].Get<uint8>();
    uint8 team           = f[3].Get<uint8>();
    std::string chanLbl  = f[4].Get<std::string>();
    std::string chanName = f[5].Get<std::string>();
    std::string word     = f[6].Get<std::string>();
    std::string msg      = f[7].Get<std::string>();

    // Si es canal con nombre, mostrarlo: [CHANNEL: General]
    std::string chanDisplay = chanName.empty()
        ? chanLbl
        : chanLbl + ": " + chanName;

    return std::string("|cFF90EE90[") + sentAt + "]|r "
        + std::string("[") + WC_ClassColor(cls) + name + "|r]"
        + std::string("[") + WC_ClassIcon(cls) + "]["
        + WC_FactionIcon(team) + "] "
        + ChannelLabelToColor(chanLbl) + "[" + chanDisplay + "]|r "
        + "|cFFFF0000[" + word + "]|r "
        + ChannelLabelToColor(chanLbl) + "[" + msg + "]|r";
}

static std::string BuildRowByWord(Field* f)
{
    std::string sentAt   = f[0].Get<std::string>();
    std::string name     = f[1].Get<std::string>();
    uint8 cls            = f[2].Get<uint8>();
    uint8 team           = f[3].Get<uint8>();
    std::string chanLbl  = f[4].Get<std::string>();
    std::string chanName = f[5].Get<std::string>();
    std::string word     = f[6].Get<std::string>();
    std::string msg      = f[7].Get<std::string>();

    std::string chanDisplay = chanName.empty()
        ? chanLbl
        : chanLbl + ": " + chanName;

    return std::string("|cFFFF0000[") + word + "]|r "
        + std::string("[") + WC_ClassColor(cls) + name + "|r]"
        + std::string("[") + WC_ClassIcon(cls) + "]["
        + WC_FactionIcon(team) + "] "
        + ChannelLabelToColor(chanLbl) + "[" + chanDisplay + "]|r "
        + ChannelLabelToColor(chanLbl) + "[" + msg + "]|r "
        + "|cFF90EE90[" + sentAt + "]|r";
}

static std::string BuildRowByCharacter(Field* f)
{
    std::string sentAt   = f[0].Get<std::string>();
    std::string name     = f[1].Get<std::string>();
    uint8 cls            = f[2].Get<uint8>();
    uint8 team           = f[3].Get<uint8>();
    std::string chanLbl  = f[4].Get<std::string>();
    std::string chanName = f[5].Get<std::string>();
    std::string word     = f[6].Get<std::string>();
    std::string msg      = f[7].Get<std::string>();

    std::string chanDisplay = chanName.empty()
        ? chanLbl
        : chanLbl + ": " + chanName;

    return std::string("[") + WC_ClassColor(cls) + name + "|r]"
        + std::string("[") + WC_ClassIcon(cls) + "]["
        + WC_FactionIcon(team) + "] "
        + "|cFFFF0000[" + word + "]|r "
        + ChannelLabelToColor(chanLbl) + "[" + chanDisplay + "]|r "
        + ChannelLabelToColor(chanLbl) + "[" + msg + "]|r "
        + "|cFF90EE90[" + sentAt + "]|r";
}

static void PrintHeader(ChatHandler* handler, const std::string& title)
{
    handler->SendSysMessage("|cFF00CCFF[BigBro]|r " + title);
    handler->SendSysMessage(
        "|cFFFFFF00[BigBro]|r Solo se muestran las últimas 100 líneas. "
        "Usa |cFF00CCFF.bigbro blacklist reload|r si agregaste palabras nuevas.");
}

static void PrintRows(ChatHandler* handler, QueryResult result,
    std::string (*buildFn)(Field*))
{
    if (!result)
    {
        handler->SendSysMessage("|cFFFFFF00[BigBro]|r No se encontraron registros.");
        return;
    }
    do { handler->SendSysMessage(buildFn(result->Fetch())); }
    while (result->NextRow());
}

static bool CheckLevel(ChatHandler* handler)
{
    if (handler->GetSession() &&
        handler->GetSession()->GetSecurity() < AccountTypes(sWardenChat->minGMLevel))
    {
        handler->SendSysMessage("|cFFFF0000[BigBro]|r No tienes permisos para usar este comando.");
        return false;
    }
    return true;
}

// SEL eliminado — cada query tiene su propio SELECT con channel_name incluido

// ============================================================
//  WorldScript
// ============================================================
class WC_WorldScript : public WorldScript
{
public:
    WC_WorldScript() : WorldScript("WC_WorldScript") {}

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        sWardenChat->LoadConfig();
    }

    void OnStartup() override
    {
        if (!sWardenChat->enabled)
            return;
        sWardenChat->LoadBlacklist();
        sWardenChat->CleanupExpired();
        sWardenChat->CleanupWhispers();
        LOG_INFO("module", "mod-warden-chat: Módulo iniciado correctamente.");
    }
};

// ============================================================
//  PlayerScript
// ============================================================
class WC_PlayerScript : public PlayerScript
{
public:
    WC_PlayerScript() : PlayerScript("WC_PlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_CAN_PLAYER_USE_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_GROUP_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_GUILD_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_CHANNEL_CHAT,
    }) {}

    void OnPlayerLogin(Player* player) override
    {
        if (!sWardenChat->enabled) return;
        ChatHandler(player->GetSession()).SendSysMessage(
            "|cFF00CCFF[BigBro]|r |cFF00FF00El Gran Hermano te observa. "
            "Tu chat puede ser monitoreado. |TInterface\\Icons\\Spell_Holy_MagicalSentry:14:14|t|r");
    }

    void OnPlayerLogout(Player* player) override
    {
        uint32 g = player->GetGUID().GetCounter();
        sWardenChat->WatchRemoveGM(g);
        sWardenChat->WatchGroupRemove(g);
    }

    bool OnPlayerCanUseChat(Player* p, uint32 type, uint32 lang,
        std::string& msg) override
    { return Handle(p, type, lang, msg, nullptr); }

    bool OnPlayerCanUseChat(Player* p, uint32 type, uint32 lang,
        std::string& msg, Player* receiver) override
    { return Handle(p, type, lang, msg, receiver); }

    bool OnPlayerCanUseChat(Player* p, uint32 type, uint32 lang,
        std::string& msg, Group* /*g*/) override
    { return Handle(p, type, lang, msg, nullptr); }

    bool OnPlayerCanUseChat(Player* p, uint32 type, uint32 lang,
        std::string& msg, Guild* /*g*/) override
    { return Handle(p, type, lang, msg, nullptr); }

    bool OnPlayerCanUseChat(Player* p, uint32 type, uint32 lang,
        std::string& msg, Channel* c) override
    {
        std::string chanName = c ? c->GetName() : "";
        return Handle(p, type, lang, msg, nullptr, chanName);
    }

private:
    bool Handle(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver, const std::string& channelNameStr = "")
    {
        if (!sWardenChat->enabled) return true;

        // Ignorar mensajes de addons (HealBot, DBM, WeakAuras, etc.)
        if (lang == LANG_ADDON)
            return true;

        uint32 guid = player->GetGUID().GetCounter();

        // Watch
        std::set<uint32> gms = sWardenChat->WatchGetGMs(guid);
        if (!gms.empty())
        {
            std::string receiverPart = "";
            if (receiver && type == CHAT_MSG_WHISPER)
                receiverPart = " → ["
                    + std::string(WC_ClassColor(receiver->getClass()))
                    + receiver->GetName() + "|r]["
                    + WC_FactionIcon(receiver->GetTeamId()) + "]";

            // Hora actual formateada HH:MM:SS
            time_t now = time(nullptr);
            char timeBuf[10];
            strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", localtime(&now));

            std::string line =
                "|cFFFF4500[WATCH]|r "
                + std::string("[") + WC_ClassColor(player->getClass())
                + player->GetName() + "|r]["
                + WC_FactionIcon(player->GetTeamId()) + "]"
                + receiverPart + " "
                + std::string(WC_ChannelColor(type)) + "["
                + WC_ChannelLabel(type) + "]|r "
                + std::string(WC_ChannelColor(type)) + "[" + msg + "]|r "
                + "|cFF90EE90[" + timeBuf + "]|r";

            for (uint32 gmGuid : gms)
                if (Player* gm = ObjectAccessor::FindPlayerByLowGUID(gmGuid))
                    ChatHandler(gm->GetSession()).SendSysMessage(line);
                else
                    sWardenChat->WatchRemoveGM(gmGuid);
        }

        // Log completo de susurros
        if (type == CHAT_MSG_WHISPER && receiver && sWardenChat->whisperLogEnabled)
        {
            sWardenChat->SaveWhisper(
                guid,
                player->GetName(),
                player->getClass(),
                player->GetTeamId(),
                receiver->GetGUID().GetCounter(),
                receiver->GetName(),
                receiver->getClass(),
                receiver->GetTeamId(),
                msg);
        }

        // Watch bidireccional — SAY/YELL
        // Si el emisor no está siendo vigilado directamente,
        // verificamos si hay algún vigilado cerca que escucharía este mensaje
        if (type == CHAT_MSG_SAY || type == CHAT_MSG_YELL)
        {
            float range = (type == CHAT_MSG_SAY) ? 25.0f : 300.0f;
            auto allWatched = sWardenChat->WatchGetAllPairs(); // target → set<gms>

            for (const auto& [targetGuid, gmSet] : allWatched)
            {
                // Si el emisor ES el vigilado, ya lo capturó el bloque Watch arriba
                if (targetGuid == guid) continue;

                Player* target = ObjectAccessor::FindPlayerByLowGUID(targetGuid);
                if (!target || !target->IsInWorld()) continue;

                // ¿Están en el mismo mapa y dentro del rango?
                if (target->GetMapId() != player->GetMapId()) continue;
                if (player->GetDistance(target) > range) continue;

                // El vigilado escucharía este mensaje — mandar copia a sus GMs
                time_t nowB = time(nullptr);
                char timeBufB[10];
                strftime(timeBufB, sizeof(timeBufB), "%H:%M:%S", localtime(&nowB));

                std::string bidiLine =
                    "|cFFFF4500[WATCH→" + std::string(WC_ClassColor(target->getClass()))
                    + target->GetName() + "|r" + "]|r "
                    + std::string("[") + WC_ClassColor(player->getClass())
                    + player->GetName() + "|r]["
                    + WC_FactionIcon(player->GetTeamId()) + "] "
                    + std::string(WC_ChannelColor(type)) + "["
                    + WC_ChannelLabel(type) + "]|r "
                    + std::string(WC_ChannelColor(type)) + "[" + msg + "]|r "
                    + "|cFF90EE90[" + timeBufB + "]|r";

                for (uint32 gmGuid : gmSet)
                    if (Player* gm = ObjectAccessor::FindPlayerByLowGUID(gmGuid))
                        ChatHandler(gm->GetSession()).SendSysMessage(bidiLine);
            }
        }

        // Watch bidireccional — WHISPER entrante al vigilado
        // Si alguien le susurra a un vigilado, el GM también lo ve
        if (type == CHAT_MSG_WHISPER && receiver)
        {
            uint32 receiverGuid = receiver->GetGUID().GetCounter();
            std::set<uint32> receiverGMs = sWardenChat->WatchGetGMs(receiverGuid);

            if (!receiverGMs.empty())
            {
                time_t nowW = time(nullptr);
                char timeBufW[10];
                strftime(timeBufW, sizeof(timeBufW), "%H:%M:%S", localtime(&nowW));

                std::string whisperLine =
                    "|cFFFF4500[WATCH→" + std::string(WC_ClassColor(receiver->getClass()))
                    + receiver->GetName() + "|r]|r "
                    + std::string("[") + WC_ClassColor(player->getClass())
                    + player->GetName() + "|r]["
                    + WC_FactionIcon(player->GetTeamId()) + "]"
                    + " → [" + WC_ClassColor(receiver->getClass())
                    + receiver->GetName() + "|r]["
                    + WC_FactionIcon(receiver->GetTeamId()) + "] "
                    + "|cFFFF80FF[WHISPER]|r "
                    + "|cFFFF80FF[" + msg + "]|r "
                    + "|cFF90EE90[" + timeBufW + "]|r";

                for (uint32 gmGuid : receiverGMs)
                    if (Player* gm = ObjectAccessor::FindPlayerByLowGUID(gmGuid))
                        ChatHandler(gm->GetSession()).SendSysMessage(whisperLine);
            }
        }

        // WatchGroup — interceptar mensajes de grupo/raid/hermandad
        // para GMs que vigilan a alguien del mismo grupo
        if (type == CHAT_MSG_PARTY      || type == CHAT_MSG_PARTY_LEADER  ||
            type == CHAT_MSG_RAID       || type == CHAT_MSG_RAID_LEADER    ||
            type == CHAT_MSG_RAID_WARNING || type == CHAT_MSG_GUILD        ||
            type == CHAT_MSG_OFFICER)
        {
            Group* grp = player->GetGroup();
            auto wgAll = sWardenChat->WatchGroupGetAll();

            for (const auto& [gmGuid, anchorGuid] : wgAll)
            {
                // Verificar si el ancla está en el mismo grupo que el emisor
                Player* anchor = ObjectAccessor::FindPlayerByLowGUID(anchorGuid);
                if (!anchor)
                {
                    sWardenChat->WatchGroupRemove(gmGuid);
                    continue;
                }

                bool sameGroup = false;
                if (type == CHAT_MSG_GUILD || type == CHAT_MSG_OFFICER)
                {
                    // Hermandad: mismo guildId
                    sameGroup = (player->GetGuildId() != 0 &&
                                 player->GetGuildId() == anchor->GetGuildId());
                }
                else if (grp)
                {
                    // Grupo/Raid: mismo Group*
                    sameGroup = grp->IsMember(anchor->GetGUID());
                }

                if (!sameGroup) continue;

                // No enviar al GM si es él mismo quien habla
                if (gmGuid == guid) continue;

                Player* gm = ObjectAccessor::FindPlayerByLowGUID(gmGuid);
                if (!gm) { sWardenChat->WatchGroupRemove(gmGuid); continue; }

                time_t nowWG = time(nullptr);
                char timeBufWG[10];
                strftime(timeBufWG, sizeof(timeBufWG), "%H:%M:%S", localtime(&nowWG));

                std::string wgLine =
                    "|cFF00CCFF[WGROUP]|r "
                    + std::string("[") + WC_ClassColor(player->getClass())
                    + player->GetName() + "|r]["
                    + WC_FactionIcon(player->GetTeamId()) + "] "
                    + std::string(WC_ChannelColor(type)) + "["
                    + WC_ChannelLabel(type) + "]|r "
                    + std::string(WC_ChannelColor(type)) + "[" + msg + "]|r "
                    + "|cFF90EE90[" + timeBufWG + "]|r";

                ChatHandler(gm->GetSession()).SendSysMessage(wgLine);
            }
        }

        // Blacklist
        const BlacklistWord* bw = sWardenChat->Check(msg);
        if (!bw) return true;

        bool sanctionApplied = false;
        bool shouldBlock     = bw->block || sWardenChat->blockEnabled;
        bool shouldReplace   = !shouldBlock && sWardenChat->replaceEnabled;
        std::string replText = shouldReplace
            ? (!bw->replacement.empty() ? bw->replacement : sWardenChat->replaceText)
            : "";

        if (bw->sanction && sWardenChat->muteEnabled)
        {
            player->GetSession()->m_muteTime = time(nullptr) + sWardenChat->muteDuration;
            ChatHandler(player->GetSession()).SendSysMessage(
                "|cFF00CCFF[BigBro]|r |cFFFF0000Has sido silenciado automáticamente. "
                "Duración: " + std::to_string(sWardenChat->muteDuration) + " segundos.|r");
            sanctionApplied = true;
        }

        if (shouldBlock)
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "|cFF00CCFF[BigBro]|r |cFFFF0000Tu mensaje ha sido bloqueado "
                "por contener lenguaje no permitido.|r");
            sWardenChat->SaveFlagged(guid, player->GetName(),
                player->getClass(), player->GetTeamId(),
                WC_ChannelLabel(type), channelNameStr,
                bw->word, sanctionApplied, msg);
            return false;
        }

        if (shouldReplace && !replText.empty())
        {
            msg = replText;
            ChatHandler(player->GetSession()).SendSysMessage(
                "|cFF00CCFF[BigBro]|r |cFFFFFF00Tu mensaje ha sido modificado "
                "por contener lenguaje no permitido.|r");
        }
        else if (!sanctionApplied)
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "|cFF00CCFF[BigBro]|r |cFF00FF00Tu mensaje ha sido registrado "
                "y podría ser revisado por un moderador.|r");
        }

        sWardenChat->SaveFlagged(guid, player->GetName(),
            player->getClass(), player->GetTeamId(),
            WC_ChannelLabel(type), channelNameStr,
            bw->word, sanctionApplied, msg);

        return true;
    }
};

// ============================================================
//  CommandScript — .bigbro blacklist + .bigbro watch
// ============================================================
class WC_CommandScript : public CommandScript
{
public:
    WC_CommandScript() : CommandScript("WC_CommandScript") {}

    ChatCommandTable GetCommands() const override
    {


        static ChatCommandTable lookupTable =
        {
            { "word",      HandleLookupWord,      SEC_GAMEMASTER, Console::No },
            { "character", HandleLookupCharacter, SEC_GAMEMASTER, Console::No },
            { "",          HandleLookupHelp,      SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable addTable =
        {
            { "", HandleAddWord, SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable blacklistTable =
        {
            { "logs",   HandleShowAll,   SEC_GAMEMASTER, Console::No      },
            { "lookup", lookupTable                                        },
            { "add",    addTable                                           },
            { "count",  HandleCount,         SEC_GAMEMASTER, Console::No  },
            { "reload", HandleReload,        SEC_GAMEMASTER, Console::No  },
            { "",       HandleBlacklistHelp, SEC_GAMEMASTER, Console::No  },
        };

        static ChatCommandTable watchTable =
        {
            { "on",   HandleWatchOn,      SEC_GAMEMASTER, Console::No },
            { "off",  HandleWatchOff,     SEC_GAMEMASTER, Console::No },
            { "list", HandleWatchList,    SEC_GAMEMASTER, Console::No },
            { "",     HandleWatchHelp,    SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable watchGroupTable =
        {
            { "on",   HandleWatchGroupOn,   SEC_GAMEMASTER, Console::No },
            { "off",  HandleWatchGroupOff,  SEC_GAMEMASTER, Console::No },
            { "list", HandleWatchGroupList, SEC_GAMEMASTER, Console::No },
            { "",     HandleWatchGroupHelp, SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable whisperTable =
        {
            { "from",    HandleWhisperShow,    SEC_GAMEMASTER, Console::No },
            { "between", HandleWhisperBetween, SEC_GAMEMASTER, Console::No },
            { "",        HandleWhisperHelp,    SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable bbTable =
        {
            { "blacklist",  blacklistTable                                },
            { "watch",      watchTable                                    },
            { "watchgroup", watchGroupTable                               },
            { "whisper",    whisperTable                                  },
            { "",           HandleBigBroHelp, SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable rootTable =
        {
            { "bigbro", bbTable },
        };

        return rootTable;
    }

private:

    // ── Ayuda ──────────────────────────────────────────────
    static bool HandleBigBroHelp(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Comandos disponibles:");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist|r — Gestión y consulta de mensajes etiquetados");
        handler->SendSysMessage("  |cFFFFFF00.bigbro watch|r          — Vigilancia individual en tiempo real");
        handler->SendSysMessage("  |cFFFFFF00.bigbro watchgroup|r     — Vigilancia de grupo/raid/hermandad completo");
        handler->SendSysMessage("  |cFFFFFF00.bigbro whisper|r        — Consultar log de susurros entre jugadores");
        return true;
    }

    static bool HandleBlacklistHelp(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Subcomandos de |cFFFFFF00blacklist|r:");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist logs|r          — Últimos 100 mensajes");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist lookup word <p>|r   — Buscar por palabra");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist lookup character <n>|r — Buscar por jugador");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist count|r             — Estadísticas 24h");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist add <p> <0|1>|r — Añadir palabra");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist reload|r            — Recargar blacklist");
        return true;
    }

    static bool HandleLookupHelp(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Subcomandos de |cFFFFFF00lookup|r:");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist lookup word <palabra>|r      — Por palabra prohibida");
        handler->SendSysMessage("  |cFFFFFF00.bigbro blacklist lookup character <nombre>|r  — Por nombre de jugador");
        return true;
    }

    static bool HandleAddHelp(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Uso: |cFFFFFF00.bigbro blacklist add <palabra> <0|1>|r");
        handler->SendSysMessage("  |cFFAAAAAA0|r = solo registrar  |  |cFFAAAAAA1|r = registrar + mute automático");
        handler->SendSysMessage("  Ej: |cFFAAAAAA.bigbro blacklist add gold 0|r");
        handler->SendSysMessage("  Ej: |cFFAAAAAA.bigbro blacklist add exploit 1|r");
        return true;
    }

    static bool HandleWatchHelp(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        handler->SendSysMessage("|cFFFF4500[BigBro]|r Subcomandos de |cFFFFFF00watch|r:");
        handler->SendSysMessage("  |cFFFFFF00.bigbro watch on <nombre>|r  — Vigilar todos los mensajes de un jugador");
        handler->SendSysMessage("  |cFFFFFF00.bigbro watch off <nombre>|r — Dejar de vigilar");
        handler->SendSysMessage("  |cFFFFFF00.bigbro watch list|r         — Ver jugadores vigilados actualmente");
        return true;
    }

    // ── Blacklist: show all ────────────────────────────────
    static bool HandleShowAll(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        PrintHeader(handler, "Últimos 100 mensajes etiquetados:");

        QueryResult result = LoginDatabase.Query(
            "SELECT sent_at, sender_name, sender_class, sender_team, "
            "channel_label, channel_name, triggered_word, LEFT(message, 60) "
            "FROM warden_flagged_messages ORDER BY sent_at DESC LIMIT 100");

        PrintRows(handler, result, BuildRowByDate);
        return true;
    }

    // ── Blacklist: lookup word ─────────────────────────────
    static bool HandleLookupWord(ChatHandler* handler, Optional<std::string> wordArg)
    {
        if (!CheckLevel(handler)) return false;
        if (!wordArg)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Uso: |cFFFFFF00.bigbro blacklist lookup word <palabra>|r");
            return true;
        }
        std::string word = *wordArg;
        PrintHeader(handler, "Búsqueda por palabra: |cFFFF0000" + word + "|r");
        LoginDatabase.EscapeString(word);

        QueryResult result = LoginDatabase.Query(
            "SELECT sent_at, sender_name, sender_class, sender_team, "
            "channel_label, channel_name, triggered_word, LEFT(message, 60) "
            "FROM warden_flagged_messages "
            "WHERE triggered_word = '{}' ORDER BY sent_at DESC LIMIT 100", word);

        PrintRows(handler, result, BuildRowByWord);
        return true;
    }

    // ── Blacklist: lookup character ────────────────────────
    static bool HandleLookupCharacter(ChatHandler* handler, Optional<std::string> nameArg)
    {
        if (!CheckLevel(handler)) return false;
        if (!nameArg)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Uso: |cFFFFFF00.bigbro blacklist lookup character <nombre>|r");
            return true;
        }
        std::string playerName = *nameArg;
        PrintHeader(handler, "Búsqueda por jugador: |cFFFFFF00" + playerName + "|r");
        LoginDatabase.EscapeString(playerName);

        QueryResult result = LoginDatabase.Query(
            "SELECT sent_at, sender_name, sender_class, sender_team, "
            "channel_label, channel_name, triggered_word, LEFT(message, 60) "
            "FROM warden_flagged_messages "
            "WHERE sender_name = '{}' ORDER BY sent_at DESC LIMIT 100", playerName);

        PrintRows(handler, result, BuildRowByCharacter);
        return true;
    }

    // ── Blacklist: count ───────────────────────────────────
    static bool HandleCount(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;

        QueryResult total = LoginDatabase.Query(
            "SELECT COUNT(*) FROM warden_flagged_messages "
            "WHERE sent_at >= NOW() - INTERVAL 24 HOUR");

        uint64 totalCount = 0;
        if (total)
            totalCount = total->Fetch()[0].Get<uint64>();

        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Mensajes flagueados en las últimas 24h:");
        handler->SendSysMessage("|cFF00FF00Total: " + std::to_string(totalCount) + " mensajes|r");

        if (totalCount == 0) return true;

        QueryResult bd = LoginDatabase.Query(
            "SELECT triggered_word, COUNT(*) cnt "
            "FROM warden_flagged_messages "
            "WHERE sent_at >= NOW() - INTERVAL 24 HOUR "
            "GROUP BY triggered_word ORDER BY cnt DESC");

        if (!bd) return true;

        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Desglose por palabra:");
        do
        {
            Field* f = bd->Fetch();
            handler->SendSysMessage(
                "  |cFFFF0000" + WC_ToUpper(f[0].Get<std::string>()) + "|r "
                + "|cFFFF80FF" + std::to_string(f[1].Get<uint64>()) + "|r "
                + "|cFFB048F8vez(ces)|r");
        } while (bd->NextRow());

        return true;
    }

    // ── Blacklist: add word ────────────────────────────────
    static bool HandleAddWord(ChatHandler* handler, Optional<std::string> wordArg, Optional<uint32> sanctionArg)
    {
        if (!CheckLevel(handler)) return false;

        if (!wordArg || !sanctionArg)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Uso: |cFFFFFF00.bigbro blacklist add <palabra> <0|1>|r");
            handler->SendSysMessage("  |cFFAAAAAA0|r = solo registrar  |  |cFFAAAAAA1|r = registrar + mute");
            return true;
        }

        std::string word    = *wordArg;
        uint32      sanction = *sanctionArg;

        if (sanction > 1)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r El segundo argumento debe ser |cFFFFFF000|r o |cFFFFFF001|r.");
            return true;
        }

        std::string eWord = word;
        LoginDatabase.EscapeString(eWord);

        if (LoginDatabase.Query("SELECT id FROM warden_blacklist WHERE word = '{}'", eWord))
        {
            handler->SendSysMessage("|cFFFFFF00[BigBro]|r La palabra |cFFFF0000" + word + "|r ya existe.");
            return true;
        }

        LoginDatabase.Execute(
            "INSERT INTO warden_blacklist (word, sanction, added_by) VALUES ('{}', {}, '{}')",
            eWord, sanction,
            handler->GetSession() ? handler->GetSession()->GetPlayer()->GetName() : "console");

        sWardenChat->LoadBlacklist();

        handler->SendSysMessage(
            "|cFF00FF00[BigBro]|r Palabra |cFFFF0000" + word + "|r añadida "
            + std::string(sanction ? "|cFFFF0000con mute|r" : "|cFF00FF00sin mute|r") + ".");
        return true;
    }

    // ── Blacklist: reload ──────────────────────────────────
    static bool HandleReload(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        sWardenChat->LoadBlacklist();
        handler->SendSysMessage("|cFF00FF00[BigBro]|r Blacklist recargada correctamente.");
        return true;
    }

    // ── Watch: on ─────────────────────────────────────────
    static bool HandleWatchOn(ChatHandler* handler, Optional<std::string> playerNameArg)
    {
        if (!CheckLevel(handler)) return false;

        if (!playerNameArg)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Uso: |cFFFFFF00.bigbro watch on <nombre>|r");
            return true;
        }

        std::string playerName = *playerNameArg;
        Player* target = ObjectAccessor::FindPlayerByName(playerName);
        if (!target)
        {
            handler->SendSysMessage(
                "|cFFFF0000[BigBro]|r Jugador |cFFFFFF00" + playerName + "|r no encontrado o no está conectado.");
            return true;
        }

        uint32 gmGuid     = handler->GetSession()->GetPlayer()->GetGUID().GetCounter();
        uint32 targetGuid = target->GetGUID().GetCounter();

        // Verificar si el objetivo es GM y si está permitido chuzar GMs
        if (target->GetSession()->GetSecurity() > SEC_PLAYER)
        {
            if (!sWardenChat->watchCanSpyGMs)
            {
                handler->SendSysMessage(
                    "|cFFFF0000[BigBro]|r No está permitido vigilar a otros GMs. "
                    "Activa |cFFFFFF00WardenChat.Watch.AllowSpyOnGMs|r en el conf.");
                return true;
            }
            // Verificar si el GM que chuzea tiene nivel suficiente
            uint8 spyerLevel  = static_cast<uint8>(handler->GetSession()->GetSecurity());
            uint8 targetLevel = static_cast<uint8>(target->GetSession()->GetSecurity());
            if (spyerLevel < sWardenChat->watchMinGMToSpy)
            {
                handler->SendSysMessage(
                    "|cFFFF0000[BigBro]|r No tienes nivel suficiente para vigilar a otros GMs.");
                return true;
            }
            // Opcional: avisar que se está vigilando a un GM de mayor rango
            if (targetLevel >= spyerLevel)
            {
                handler->SendSysMessage(
                    "|cFFFFFF00[BigBro]|r Advertencia: estás vigilando a un GM de igual o mayor rango.");
            }
        }

        sWardenChat->WatchAdd(gmGuid, targetGuid);

        handler->SendSysMessage(
            "|cFFFF4500[BigBro]|r Vigilancia activada: "
            + std::string(WC_ClassColor(target->getClass()))
            + playerName + "|r["
            + WC_FactionIcon(target->GetTeamId())
            + "] |cFFAAAAAA— recibirás copia de todos sus mensajes.|r");
        return true;
    }

    // ── Watch: off ────────────────────────────────────────
    static bool HandleWatchOff(ChatHandler* handler, Optional<std::string> playerNameArg)
    {
        if (!CheckLevel(handler)) return false;

        if (!playerNameArg)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Uso: |cFFFFFF00.bigbro watch off <nombre>|r");
            return true;
        }

        std::string playerName = *playerNameArg;
        Player* target = ObjectAccessor::FindPlayerByName(playerName);
        if (!target)
        {
            handler->SendSysMessage(
                "|cFFFF0000[BigBro]|r Jugador |cFFFFFF00" + playerName + "|r no encontrado o no está conectado.");
            return true;
        }

        sWardenChat->WatchRemove(
            handler->GetSession()->GetPlayer()->GetGUID().GetCounter(),
            target->GetGUID().GetCounter());

        handler->SendSysMessage(
            "|cFFFF4500[BigBro]|r Vigilancia desactivada: |cFFFFFF00" + playerName + "|r.");
        return true;
    }

    // ── Watch: list ───────────────────────────────────────
    static bool HandleWatchList(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;

        uint32 gmGuid = handler->GetSession()->GetPlayer()->GetGUID().GetCounter();
        std::vector<uint32> targets = sWardenChat->WatchGetTargets(gmGuid);

        if (targets.empty())
        {
            handler->SendSysMessage("|cFFFFFF00[BigBro]|r No estás vigilando a nadie actualmente.");
            return true;
        }

        handler->SendSysMessage("|cFFFF4500[BigBro]|r Jugadores bajo vigilancia:");
        for (uint32 guid : targets)
        {
            if (Player* t = ObjectAccessor::FindPlayerByLowGUID(guid))
                handler->SendSysMessage(
                    "  [" + std::string(WC_ClassColor(t->getClass()))
                    + t->GetName() + "|r]["
                    + WC_FactionIcon(t->GetTeamId()) + "]");
            else
                handler->SendSysMessage(
                    "  |cFFAAAAAA[Desconectado — GUID: " + std::to_string(guid) + "]|r");
        }
        return true;
    }

    // ── Whisper: helpers de display ──────────────────────
    // Campos: f[0]=sent_at f[1]=sender_name f[2]=sender_class f[3]=sender_team
    //         f[4]=receiver_name f[5]=receiver_class f[6]=receiver_team f[7]=message

    static std::string BuildWhisperRow(Field* f)
    {
        std::string sentAt       = f[0].Get<std::string>();
        std::string senderName   = f[1].Get<std::string>();
        uint8       senderClass  = f[2].Get<uint8>();
        uint8       senderTeam   = f[3].Get<uint8>();
        std::string receiverName = f[4].Get<std::string>();
        uint8       receiverClass= f[5].Get<uint8>();
        uint8       receiverTeam = f[6].Get<uint8>();
        std::string message      = f[7].Get<std::string>();

        // [fecha] [Emisor🔴] → [Receptor🔵] [mensaje en color whisper]
        return std::string("|cFF90EE90[") + sentAt + "]|r "
            + std::string("[") + WC_ClassColor(senderClass) + senderName + "|r]["
            + WC_ClassIcon(senderClass) + "][" + WC_FactionIcon(senderTeam) + "]"
            + " → "
            + std::string("[") + WC_ClassColor(receiverClass) + receiverName + "|r]["
            + WC_ClassIcon(receiverClass) + "][" + WC_FactionIcon(receiverTeam) + "] "
            + "|cFFFF7EFF[" + message + "]|r";
    }

    // ── Whisper: help ─────────────────────────────────────
    static bool HandleWhisperHelp(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Subcomandos de |cFFFFFF00whisper|r:");
        handler->SendSysMessage("  |cFFFFFF00.bigbro whisper from <nombre>|r             — Susurros enviados y recibidos por un jugador");
        handler->SendSysMessage("  |cFFFFFF00.bigbro whisper between <nombre1> <nombre2>|r — Conversación entre dos jugadores");
        return true;
    }

    // ── Whisper: show <nombre> ────────────────────────────
    // Muestra todos los susurros donde el jugador es emisor O receptor
    static bool HandleWhisperShow(ChatHandler* handler, Optional<std::string> nameArg)
    {
        if (!CheckLevel(handler)) return false;
        if (!nameArg)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Uso: |cFFFFFF00.bigbro whisper from <nombre>|r");
            return true;
        }

        std::string name = *nameArg;
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Susurros de/hacia |cFFFFFF00" + name + "|r (últimos 100):");
        handler->SendSysMessage("|cFFFFFF00[BigBro]|r Formato: |cFF90EE90[fecha]|r [Emisor] → [Receptor] |cFFFF7EFF[mensaje]|r");

        LoginDatabase.EscapeString(name);

        QueryResult result = LoginDatabase.Query(
            "SELECT sent_at, sender_name, sender_class, sender_team, "
            "receiver_name, receiver_class, receiver_team, LEFT(message, 60) "
            "FROM warden_whisper_log "
            "WHERE sender_name = '{}' OR receiver_name = '{}' "
            "ORDER BY sent_at DESC LIMIT 100",
            name, name);

        if (!result)
        {
            handler->SendSysMessage("|cFFFFFF00[BigBro]|r No se encontraron susurros.");
            return true;
        }
        do { handler->SendSysMessage(BuildWhisperRow(result->Fetch())); }
        while (result->NextRow());
        return true;
    }

    // ── Whisper: between <nombre1> <nombre2> ─────────────
    // Muestra solo la conversación entre dos jugadores específicos
    static bool HandleWhisperBetween(ChatHandler* handler,
        Optional<std::string> name1Arg, Optional<std::string> name2Arg)
    {
        if (!CheckLevel(handler)) return false;
        if (!name1Arg || !name2Arg)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Uso: |cFFFFFF00.bigbro whisper between <nombre1> <nombre2>|r");
            return true;
        }

        std::string name1 = *name1Arg;
        std::string name2 = *name2Arg;

        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Conversación entre |cFFFFFF00"
            + name1 + "|r y |cFFFFFF00" + name2 + "|r (últimos 100):");
        handler->SendSysMessage("|cFFFFFF00[BigBro]|r Formato: |cFF90EE90[fecha]|r [Emisor] → [Receptor] |cFFFF7EFF[mensaje]|r");

        LoginDatabase.EscapeString(name1);
        LoginDatabase.EscapeString(name2);

        QueryResult result = LoginDatabase.Query(
            "SELECT sent_at, sender_name, sender_class, sender_team, "
            "receiver_name, receiver_class, receiver_team, LEFT(message, 60) "
            "FROM warden_whisper_log "
            "WHERE (sender_name = '{}' AND receiver_name = '{}') "
            "   OR (sender_name = '{}' AND receiver_name = '{}') "
            "ORDER BY sent_at ASC LIMIT 100",
            name1, name2, name2, name1);

        if (!result)
        {
            handler->SendSysMessage("|cFFFFFF00[BigBro]|r No se encontraron susurros entre estos jugadores.");
            return true;
        }
        do { handler->SendSysMessage(BuildWhisperRow(result->Fetch())); }
        while (result->NextRow());
        return true;
    }

    // ── WatchGroup: help ──────────────────────────────────
    static bool HandleWatchGroupHelp(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Subcomandos de |cFFFFFF00watchgroup|r:");
        handler->SendSysMessage("  |cFFFFFF00.bigbro watchgroup on <nombre>|r  — Interceptar grupo/raid/hermandad del jugador");
        handler->SendSysMessage("  |cFFFFFF00.bigbro watchgroup off|r          — Detener interceptación de grupo");
        handler->SendSysMessage("  |cFFFFFF00.bigbro watchgroup list|r         — Ver vigilancia de grupo activa");
        return true;
    }

    // ── WatchGroup: on ────────────────────────────────────
    static bool HandleWatchGroupOn(ChatHandler* handler, Optional<std::string> nameArg)
    {
        if (!CheckLevel(handler)) return false;
        if (!nameArg)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Uso: |cFFFFFF00.bigbro watchgroup on <nombre>|r");
            return true;
        }
        Player* anchor = ObjectAccessor::FindPlayerByName(*nameArg);
        if (!anchor)
        {
            handler->SendSysMessage("|cFFFF0000[BigBro]|r Jugador |cFFFFFF00" + *nameArg + "|r no encontrado.");
            return true;
        }
        if (!anchor->GetGroup() && anchor->GetGuildId() == 0)
        {
            handler->SendSysMessage("|cFFFFFF00[BigBro]|r " + std::string(WC_ClassColor(anchor->getClass()))
                + *nameArg + "|r no pertenece a ningún grupo ni hermandad.");
            return true;
        }
        uint32 gmGuid = handler->GetSession()->GetPlayer()->GetGUID().GetCounter();
        sWardenChat->WatchGroupAdd(gmGuid, anchor->GetGUID().GetCounter());
        std::string info = "";
        if (anchor->GetGroup())   info += "|cFFFF7F00[Grupo/Raid]|r ";
        if (anchor->GetGuildId()) info += "|cFF40FF40[Hermandad]|r";
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Vigilancia de grupo activada: "
            + std::string(WC_ClassColor(anchor->getClass())) + *nameArg + "|r["
            + WC_FactionIcon(anchor->GetTeamId()) + "]. " + info);
        return true;
    }

    // ── WatchGroup: off ───────────────────────────────────
    static bool HandleWatchGroupOff(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        uint32 gmGuid = handler->GetSession()->GetPlayer()->GetGUID().GetCounter();
        if (sWardenChat->WatchGroupGetAnchor(gmGuid) == 0)
        {
            handler->SendSysMessage("|cFFFFFF00[BigBro]|r No tienes ninguna vigilancia de grupo activa.");
            return true;
        }
        sWardenChat->WatchGroupRemove(gmGuid);
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Vigilancia de grupo desactivada.");
        return true;
    }

    // ── WatchGroup: list ──────────────────────────────────
    static bool HandleWatchGroupList(ChatHandler* handler)
    {
        if (!CheckLevel(handler)) return false;
        uint32 gmGuid     = handler->GetSession()->GetPlayer()->GetGUID().GetCounter();
        uint32 anchorGuid = sWardenChat->WatchGroupGetAnchor(gmGuid);
        if (anchorGuid == 0)
        {
            handler->SendSysMessage("|cFFFFFF00[BigBro]|r No tienes ninguna vigilancia de grupo activa.");
            return true;
        }
        Player* anchor = ObjectAccessor::FindPlayerByLowGUID(anchorGuid);
        if (!anchor)
        {
            handler->SendSysMessage("|cFFFFFF00[BigBro]|r El jugador ancla se ha desconectado.");
            sWardenChat->WatchGroupRemove(gmGuid);
            return true;
        }
        handler->SendSysMessage("|cFF00CCFF[BigBro]|r Vigilancia de grupo activa:");
        handler->SendSysMessage("  Ancla: [" + std::string(WC_ClassColor(anchor->getClass()))
            + anchor->GetName() + "|r][" + WC_FactionIcon(anchor->GetTeamId()) + "]");
        if (Group* grp = anchor->GetGroup())
        {
            handler->SendSysMessage("|cFFFF7F00[BigBro]|r Miembros del grupo:");
            for (GroupReference* itr = grp->GetFirstMember(); itr; itr = itr->next())
            {
                Player* m = itr->GetSource();
                if (!m) continue;
                handler->SendSysMessage("  [" + std::string(WC_ClassColor(m->getClass()))
                    + m->GetName() + "|r][" + WC_FactionIcon(m->GetTeamId()) + "]");
            }
        }
        if (anchor->GetGuildId())
            handler->SendSysMessage("|cFF40FF40[BigBro]|r También monitoreando hermandad/oficiales.|r");
        return true;
    }
};

// ============================================================
//  Registro
// ============================================================
void AddSC_WardenChat()
{
    new WC_WorldScript();
    new WC_PlayerScript();
    new WC_CommandScript();
}
