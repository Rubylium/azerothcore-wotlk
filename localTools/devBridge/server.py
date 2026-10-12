"""The test client's remote control, as an MCP server (LOCAL DEV ONLY).

Drives the dev client (CleanWOTLK, the EVODEV account) through the client extension's dev bridge
(awesome_wotlk src/AwesomeWotlkLib/DevBridge.cpp: a socket on 127.0.0.1, off unless the client is started with
-devbridge <port> -devtoken <token>, which only this server does), and reads the server's side: its log, its
database. Started by Claude Code from the repository's .mcp.json:

    python localTools/devBridge/server.py

Tools: client_start / client_stop, lua, gm, screenshot, camera, units, position, chat, wait, server_log, db_query,
online. See each one's description. The client runs windowed on the user's desktop: never start it while they play.
"""
import base64
import io
import os
import re
import secrets
import shutil
import socket
import subprocess
import threading
import time
from pathlib import Path

from mcp.server.mcpserver import Image, MCPServer

REPO = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get('EVO_DEV_CLIENT', r'C:\Users\alexi\Documents\GitHub\CleanWOTLK'))
CONFIG = CLIENT / 'WTF' / 'Config.wtf'
SAVED_CONFIG = Path(os.environ.get('TEMP', '.')) / 'EvoDevBridge.Config.wtf'
SCREENSHOTS = CLIENT / 'Screenshots'
SERVER_LOG = REPO / 'server' / 'Server.log'
WORLD_CONF = REPO / 'server' / 'configs' / 'worldserver.conf'
MYSQL = Path(r'C:\laragon\bin\mysql\mysql-8.0.30-winx64\bin\mysql.exe')
PORT = 47901

# Run once the client is in the world: a serializer for what the Lua returns, and the chat kept for `chat`/`gm`
BOOTSTRAP = r'''
function EvaDevSerialize(value, depth)
    depth = depth or 0
    local kind = type(value)
    if kind == "table" then
        if depth > 3 then return "{...}" end
        local parts = {}
        for k, v in pairs(value) do
            parts[#parts + 1] = tostring(k) .. "=" .. EvaDevSerialize(v, depth + 1)
            if #parts > 200 then parts[#parts + 1] = "..." break end
        end
        return "{" .. table.concat(parts, ", ") .. "}"
    end
    return tostring(value)
end
if not EvaDev then
    EvaDev = { lines = {}, read = 0 }
    local frame = CreateFrame("Frame")
    for _, event in ipairs({ "CHAT_MSG_SYSTEM", "CHAT_MSG_SAY", "CHAT_MSG_YELL", "CHAT_MSG_WHISPER",
        "CHAT_MSG_MONSTER_SAY", "CHAT_MSG_MONSTER_YELL", "CHAT_MSG_MONSTER_EMOTE", "CHAT_MSG_RAID_BOSS_EMOTE",
        "CHAT_MSG_RAID_BOSS_WHISPER", "UI_ERROR_MESSAGE", "CHAT_MSG_ADDON" }) do
        frame:RegisterEvent(event)
    end
    frame:SetScript("OnEvent", function(_, event, a1, a2)
        local text = event == "CHAT_MSG_ADDON" and (tostring(a1) .. " " .. tostring(a2)) or tostring(a1)
        EvaDev.lines[#EvaDev.lines + 1] = string.format("%.1f %s %s", GetTime(), event:gsub("CHAT_MSG_", ""), text)
        if #EvaDev.lines > 500 then table.remove(EvaDev.lines, 1); EvaDev.read = math.max(0, EvaDev.read - 1) end
    end)
    function EvaDev.Drain()
        local out = {}
        for i = EvaDev.read + 1, #EvaDev.lines do out[#out + 1] = EvaDev.lines[i] end
        EvaDev.read = #EvaDev.lines
        return table.concat(out, "\n")
    end
end
if StopCinematic then StopCinematic() end
return "bootstrapped"
'''

server = MCPServer('wow-dev-client', instructions=(
    "Remote control of the Evolutions dev WoW client (CleanWOTLK, account EVODEV) and read access to the local "
    "server's log and database. Start the client with client_start (never while the user plays: the tool refuses if "
    "a client already runs from that folder), drive it with gm (GM commands, answers returned) and lua, look with "
    "screenshot and units/position (world coordinates), and client_stop when done. The FX lab: gm('.fxlab')."))


class Bridge:
    def __init__(self):
        self.sock = None
        self.token = None
        self.process = None
        self.counter = 0
        self.lock = threading.Lock()
        self.buffer = b''

    def connect(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                self.sock = socket.create_connection(('127.0.0.1', PORT), timeout=5)
                return True
            except OSError:
                if self.process and self.process.poll() is not None:
                    raise RuntimeError('the client exited before its bridge opened')
                time.sleep(1)
        return False

    def request(self, command, payload='', timeout=30.0):
        if not self.sock:
            raise RuntimeError('no client: client_start first')
        with self.lock:
            self.counter += 1
            ident = str(self.counter)
            line = f'{self.token}\t{ident}\t{command}\t{base64.b64encode(payload.encode()).decode()}\n'
            self.sock.sendall(line.encode())
            self.sock.settimeout(timeout)
            while True:
                while b'\n' not in self.buffer:
                    chunk = self.sock.recv(65536)
                    if not chunk:
                        self.sock = None
                        raise RuntimeError('the client closed the bridge (crashed or quit?)')
                    self.buffer += chunk
                raw, self.buffer = self.buffer.split(b'\n', 1)
                got_id, status, data = raw.decode().split('\t', 2)
                if got_id == ident:
                    raw_text = base64.b64decode(data)
                    try:
                        text = raw_text.decode('utf-8')
                    except UnicodeDecodeError:  # the client's name cache can hold Latin-1 names
                        text = raw_text.decode('latin-1')
                    if status != 'ok':
                        raise RuntimeError(text)
                    return text

    def lua(self, code, timeout=30.0):
        return self.request('lua', code, timeout)


bridge = Bridge()


def client_running():
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq Wow.exe', '/FO', 'CSV', '/NH'], capture_output=True,
                         text=True).stdout
    return 'Wow.exe' in out


def write_config(resolution):
    shutil.copyfile(CONFIG, SAVED_CONFIG)
    lines = [line for line in CONFIG.read_text(encoding='latin-1').splitlines()
             if not re.match(r'^SET (gxResolution|gxMaximize|gxWindow|realmName|Sound_EnableAllSound) ', line)]
    lines += ['SET gxWindow "1"', 'SET gxMaximize "0"', f'SET gxResolution "{resolution}"',
              'SET realmName "Evolutions"', 'SET Sound_EnableAllSound "0"']
    CONFIG.write_text('\n'.join(lines) + '\n', encoding='latin-1')


def restore_config():
    if SAVED_CONFIG.exists():
        shutil.copyfile(SAVED_CONFIG, CONFIG)
        SAVED_CONFIG.unlink()


@server.tool()
def client_start(character: str = 'Evoguerrier', resolution: str = '1600x900', timeout_seconds: int = 120) -> str:
    """Start the dev client logged into `character` (account EVODEV) with the dev bridge on, and wait until it is in
    the world. Refuses if a WoW client already runs (the user may be playing). Windowed, sound off."""
    if bridge.sock:
        return 'already running'
    if client_running():
        return 'refused: a WoW client is already running (the user may be playing)'
    write_config(resolution)
    bridge.token = secrets.token_hex(16)
    bridge.process = subprocess.Popen([str(CLIENT / 'Wow.exe'), '-login', 'EVODEV', '-password', 'evodev',
                                       '-realmname', 'Evolutions', '-character', character,
                                       '-devbridge', str(PORT), '-devtoken', bridge.token], cwd=str(CLIENT))
    try:
        if not bridge.connect(60):
            raise RuntimeError('the bridge never opened (is the client extension built with DevBridge?)')
        deadline = time.time() + timeout_seconds
        while time.time() < deadline:
            if 'world' in bridge.request('ping', timeout=10):
                time.sleep(2)
                bridge.lua(BOOTSTRAP)
                return f'in the world as {character}'
            time.sleep(1)
        return 'the bridge answers but the character is not in the world yet (still loading?)'
    except Exception:
        restore_config()
        raise


@server.tool()
def client_stop() -> str:
    """Quit the dev client (ForceQuit), put its Config.wtf back."""
    try:
        if bridge.sock:
            try:
                bridge.lua('ForceQuit()', timeout=5)
            except Exception:
                pass
        if bridge.process:
            try:
                bridge.process.wait(20)
            except subprocess.TimeoutExpired:
                bridge.process.kill()
    finally:
        bridge.sock = None
        bridge.process = None
        restore_config()
    return 'stopped'


@server.tool()
def lua(code: str) -> str:
    """Run Lua in the client (a chunk: `return` what you want back; several values come back one a line). Also
    available: EvaDevPosition([unit]) -> x, y, z, facing (world coordinates); EvaDevUnits([range]) -> one line per
    unit: guid|entry|name|x|y|z|facing|health|max|scale|display|emoteState."""
    return bridge.lua(code)


@server.tool()
def gm(command: str, wait_seconds: float = 0.8) -> str:
    """Send a chat line as the character (a '.command' is a GM command; whispered to itself, so it works dead too)
    and return the chat and system lines that came back within wait_seconds."""
    bridge.lua('EvaDev.Drain()')
    escaped = command.replace('\\', '\\\\').replace('"', '\\"')
    bridge.lua(f'SendChatMessage("{escaped}", "WHISPER", nil, UnitName("player"))')
    time.sleep(wait_seconds)
    return bridge.lua('return EvaDev.Drain()') or '(nothing came back)'


@server.tool()
def chat() -> str:
    """The chat and system lines received since the last call (boss yells, emotes, errors, addon messages)."""
    return bridge.lua('return EvaDev.Drain()') or '(nothing)'


@server.tool()
def screenshot(hide_ui: bool = False, width: int = 1280) -> Image:
    """A screenshot of the client, scaled to `width` pixels wide. hide_ui: the interface hidden for it."""
    before = {p.name for p in SCREENSHOTS.glob('*')} if SCREENSHOTS.exists() else set()
    if hide_ui:
        bridge.lua('UIParent:Hide()')
        time.sleep(0.2)
    bridge.lua('Screenshot()')
    deadline = time.time() + 10
    shot = None
    while time.time() < deadline and not shot:
        time.sleep(0.4)
        fresh = [p for p in SCREENSHOTS.glob('*') if p.name not in before]
        if fresh:
            candidate = max(fresh, key=lambda p: p.stat().st_mtime)
            size = candidate.stat().st_size
            time.sleep(0.3)
            if candidate.stat().st_size == size and size > 0:
                shot = candidate
    if hide_ui:
        bridge.lua('UIParent:Show()')
    if not shot:
        raise RuntimeError('no screenshot appeared')
    from PIL import Image as PILImage
    picture = PILImage.open(shot).convert('RGB')
    if picture.width > width:
        picture = picture.resize((width, int(picture.height * width / picture.width)))
    out = io.BytesIO()
    picture.save(out, format='JPEG', quality=85)
    shot.unlink()
    return Image(data=out.getvalue(), format='jpeg')


@server.tool()
def camera(preset: str = 'three_quarter', distance: float = 15.0) -> str:
    """Place the camera behind the character: preset 'three_quarter' (looking down at ~45 degrees), 'top' (straight
    down), 'level' (eye height). distance: yards from the character."""
    tilt = {'level': 0.0, 'three_quarter': 0.35, 'top': 1.2}.get(preset)
    if tilt is None:
        return 'presets: three_quarter, top, level'
    bridge.lua(f'SetView(2); CameraZoomIn(50); CameraZoomOut({distance})')
    time.sleep(0.6)
    if tilt:
        bridge.lua('MoveViewUpStart()')
        time.sleep(tilt)
        bridge.lua('MoveViewUpStop()')
    return f'camera {preset} at {distance} yd'


@server.tool()
def units(range_yards: float = 60.0) -> str:
    """Every unit near the character: guid|entry|name|x|y|z|facing|health|max|scale|display|emoteState."""
    return bridge.lua(f'return EvaDevUnits({range_yards})') or '(none)'


@server.tool()
def position(unit: str = 'player') -> str:
    """A unit's world position: x y z facing (unit: player, target, ...)."""
    return bridge.lua(f'return EvaDevPosition("{unit}")') or '(no such unit)'


@server.tool()
def wait(seconds: float) -> str:
    """Wait (the fight goes on meanwhile), at most 120 s."""
    time.sleep(max(0.0, min(seconds, 120.0)))
    return f'waited {seconds} s'


@server.tool()
def server_log(pattern: str = '', lines: int = 60) -> str:
    """The world server's log (Server.log), its last `lines` lines matching the regular expression `pattern`."""
    text = SERVER_LOG.read_text(encoding='utf-8', errors='replace').splitlines()
    if pattern:
        rx = re.compile(pattern)
        text = [line for line in text if rx.search(line)]
    return '\n'.join(text[-lines:]) or '(no match)'


def mysql(sql):
    conf = WORLD_CONF.read_text(encoding='utf-8', errors='replace')
    host, port, user, password, _ = re.search(r'^WorldDatabaseInfo\s*=\s*"([^"]+)"', conf, re.M).group(1).split(';')
    env = dict(os.environ, MYSQL_PWD=password)
    out = subprocess.run([str(MYSQL), f'--host={host}', f'--port={port}', f'--user={user}', '--protocol=TCP',
                          '--batch', '-e', sql], capture_output=True, text=True, env=env, stdin=subprocess.DEVNULL)
    if out.returncode:
        raise RuntimeError(out.stderr.strip())
    return out.stdout


@server.tool()
def db_query(sql: str) -> str:
    """A read-only query on the local databases (acore_world, acore_characters, acore_auth): SELECT, SHOW, DESCRIBE
    only. Write to the databases through pending SQL files, never here."""
    if not re.match(r'^\s*(SELECT|SHOW|DESCRIBE|EXPLAIN)\b', sql, re.I) or ';' in sql.strip().rstrip(';'):
        return 'refused: one SELECT / SHOW / DESCRIBE statement only'
    return mysql(sql) or '(no rows)'


@server.tool()
def online() -> str:
    """Who plays now (real accounts, bots and dev accounts left out) and whether a WoW client runs: check it before a
    server restart or a client start."""
    players = mysql("SELECT a.username, c.name FROM acore_characters.characters c JOIN acore_auth.account a "
                    "ON a.id = c.account WHERE c.online = 1 AND a.username NOT LIKE 'RNDBOT%' AND a.username NOT "
                    "LIKE 'VH%' AND a.username NOT LIKE 'SUB%' AND a.username NOT LIKE 'TQ%' AND "
                    "a.username <> 'EVODEV'")
    return (players.strip() or 'nobody online') + ('\nWoW running' if client_running() else '\nno WoW running')


if __name__ == '__main__':
    server.run('stdio')
