# mod-warden-chat

## Resumen hecho con IA:

Módulo de **monitoreo, auditoría y vigilancia de chat** para [AzerothCore](https://www.azerothcore.org/) WotLK 3.3.5a.

## ¿Qué hace este módulo?

`mod-warden-chat` es un sistema completo de supervisión de comunicaciones en el servidor. Permite a los administradores y moderadores:

- **Detectar y registrar** mensajes que contengan palabras prohibidas en cualquier canal de chat
- **Vigilar en tiempo real** todos los mensajes de un jugador específico
- **Interceptar** el chat de grupo, raid y hermandad completos
- **Registrar** todos los susurros entre jugadores
- **Consultar** el historial de mensajes desde el propio juego mediante comandos GM
- **Aplicar sanciones automáticas** (silencio/mute) al detectar ciertas palabras
- **Bloquear o reemplazar** mensajes con contenido no permitido

---

## Requisitos

https://www.azerothcore.org/wiki/requirements

---

## Instalación

### 1. Clonar el módulo

```bash
git clone https://github.com/M4th3m4tic4l/mod-warden-chat.git
```

### 2. Ejecutar el SQL

Conecta con tu base de datos `acore_auth` e instala el script SQL manualmente

### 3. Genera una copia del archivo conf, borra el dist del nombre y luego editalo internamente.

### 4. Recompila

## Tablas de base de datos

| Tabla | Descripción |
|---|---|
| `warden_blacklist` | Palabras prohibidas con sus configuraciones |
| `warden_flagged_messages` | Mensajes que contienen palabras prohibidas |
| `warden_whisper_log` | Log completo de susurros entre jugadores |

---

## Configuración

Edita `configs/mod-warden-chat.conf`:

```ini
# Habilitar el módulo
WardenChat.Enable = 1

# Nivel mínimo de GM para usar comandos .bigbro
# 1 = Moderator | 2 = GameMaster | 3 = Administrator
WardenChat.MinGMLevel = 2

# ── MUTE AUTOMÁTICO ──────────────────────────────────────
# Habilitar mute al detectar palabras con sanction=1
WardenChat.Mute.Enable = 0
WardenChat.Mute.DurationSeconds = 60

# ── RETENCIÓN DE MENSAJES ─────────────────────────────────
# Días que se conservan los mensajes flagueados (0 = nunca borrar)
WardenChat.Retention.Days = 90

# ── BLOQUEO DE MENSAJES ───────────────────────────────────
# Bloquear globalmente mensajes con palabras prohibidas
WardenChat.Block.Enable = 0

# ── REEMPLAZO DE MENSAJES ─────────────────────────────────
# Reemplazar mensajes flagueados por una frase alternativa
WardenChat.Replace.Enable = 0
WardenChat.Replace.Text = "Este mensaje ha sido reemplazado por contener lenguaje inapropiado."

# ── LOG DE SUSURROS ───────────────────────────────────────
WardenChat.WhisperLog.Enable = 1
WardenChat.WhisperLog.RetentionDays = 90

# ── VIGILANCIA DE GMs ─────────────────────────────────────
# Permitir que los GMs se vigilen entre sí
WardenChat.Watch.AllowSpyOnGMs = 0
# Nivel mínimo para poder vigilar a otros GMs (si AllowSpyOnGMs = 1)
WardenChat.Watch.MinLevelToSpyOnGMs = 3
```

---

## Configuración de palabras prohibidas (DB)

Cada palabra en `warden_blacklist` tiene los siguientes campos:

| Campo | Tipo | Descripción |
|---|---|---|
| `word` | VARCHAR | La palabra o frase a detectar |
| `sanction` | TINYINT | `0` = solo registrar \| `1` = mute automático |
| `block` | TINYINT | `0` = mensaje se envía \| `1` = mensaje bloqueado |
| `replacement` | VARCHAR | Frase de reemplazo. `NULL` = usar la global del .conf |
| `added_by` | VARCHAR | Nombre del GM que la añadió |

**Ejemplo:**
```sql
-- Solo registra
INSERT INTO warden_blacklist (word, sanction, block) VALUES ('spam', 0, 0);

-- Registra y mutea
INSERT INTO warden_blacklist (word, sanction, block) VALUES ('exploit', 1, 0);

-- Bloquea el mensaje completamente
INSERT INTO warden_blacklist (word, sanction, block) VALUES ('hack', 0, 1);

-- Reemplaza el mensaje por una frase personalizada
INSERT INTO warden_blacklist (word, sanction, block, replacement)
VALUES ('gold', 0, 0, 'Compra en nuestra tienda oficial.');
```

---

## Comandos in-game

Todos los comandos requieren el nivel GM configurado en `WardenChat.MinGMLevel`.

### Blacklist

| Comando | Descripción |
|---|---|
| `.bigbro blacklist logs` | Ver los últimos 100 mensajes etiquetados |
| `.bigbro blacklist lookup word <palabra>` | Buscar mensajes por palabra prohibida |
| `.bigbro blacklist lookup character <nombre>` | Buscar mensajes de un jugador |
| `.bigbro blacklist count` | Total y desglose de flags en las últimas 24h |
| `.bigbro blacklist add <palabra> <0\|1>` | Añadir palabra (0=solo log, 1=mute) |
| `.bigbro blacklist reload` | Recargar blacklist desde la DB sin reiniciar |

### Watch — Vigilancia individual

| Comando | Descripción |
|---|---|
| `.bigbro watch on <nombre>` | Vigilar todos los mensajes de un jugador en tiempo real |
| `.bigbro watch off <nombre>` | Dejar de vigilar a un jugador |
| `.bigbro watch list` | Ver jugadores actualmente vigilados |

### WatchGroup — Vigilancia de grupo

| Comando | Descripción |
|---|---|
| `.bigbro watchgroup on <nombre>` | Interceptar el chat de grupo/raid/hermandad del jugador |
| `.bigbro watchgroup off` | Detener la vigilancia de grupo |
| `.bigbro watchgroup list` | Ver el grupo actualmente vigilado y sus miembros |

### Whisper — Log de susurros

| Comando | Descripción |
|---|---|
| `.bigbro whisper from <nombre>` | Ver susurros enviados y recibidos por un jugador |
| `.bigbro whisper between <n1> <n2>` | Ver la conversación completa entre dos jugadores |

---

## Comportamiento del sistema

### Detección de palabras
El sistema busca coincidencias **parciales** — si la palabra `hack` está en la blacklist, detectará "hack", "hacker", "hacking", etc. La búsqueda es **case-insensitive**.

### Canales monitoreados
SAY · YELL · EMOTE · WHISPER · PARTY · RAID · RAID_WARNING · GUILD · OFFICER · BATTLEGROUND · CHANNEL

### Mensajes de addon
Los mensajes de addons (HealBot, DBM, WeakAuras, etc.) son ignorados automáticamente y no se registran ni se procesan.

### Vigilancia en tiempo real (Watch)
- **Unidireccional**: captura todo lo que *envía* el vigilado
- **Bidireccional SAY/YELL**: si alguien habla cerca del vigilado, el GM también lo ve
- **Bidireccional WHISPER**: si alguien le susurra al vigilado, el GM también lo ve
- Se cancela automáticamente cuando el GM se desconecta

### Vigilancia de grupo (WatchGroup)
Intercepta **todos los mensajes** de todos los miembros del grupo/raid/hermandad del jugador ancla, no solo los del jugador vigilado.

---

## Formato de los logs in-game

Los registros se muestran con codificación de colores:

- 🟢 **Fecha/hora** — verde claro
- **Nombre** — color de la clase del personaje
- **Icono de clase** — textura del cliente WoW
- **Icono de facción** — Horda/Alianza
- **Canal** — color exacto del canal en el cliente WoW
- 🔴 **Palabra prohibida** — rojo
- **Mensaje** — color del canal

---

## Licencia

Licencia pública general GNU Affero versión 3 — Ver [LICENSE](LICENSE) para más detalles.

---

## Créditos

Claude AI & AzerothCore
