import time
import math
import bzapi
from .. import Flag

flag = Flag("steamroller")

flag.name = {
    "en": "Steamroller",
    "fr": "Rouleau compresseur",
    "de": "Dampfwalze",
    "es": "Rodillo compresor",
    "pt": "Rolo compressor",
    "ja": "スチームローラー",
    "ru": "Паровой каток",
    "zh": "压路机",
}

flag.description = {
    "en": "Kills players that get too close to flag holder.",
    "fr": "Tue les joueurs qui s'approchent trop près du porteur du drapeau.",
    "de": "Tötet Spieler, die dem Flaggenträger zu nahe kommen.",
    "es": "Mata a los jugadores que se acercan demasiado al portador de la bandera.",
    "pt": "Mata jogadores que chegam perto demais do portador da bandeira.",
    "ja": "旗の所持者に近づきすぎたプレイヤーを倒します。",
    "ru": "Убивает игроков, которые подходят слишком близко к носителю флага.",
    "zh": "杀死靠近旗帜持有者过近的玩家。",
}

flag.params = {
    "radius": 3.0,
    "allow_team_kill": None,
}

_last_check: dict[int, float] = {}  # holder_player_id -> last_check_time

def _sqdist(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return (a[0]-b[0])**2 + (a[1]-b[1])**2 + (a[2]-b[2])**2

def _holder_ids() -> list[int]:
    return [
        pid for pid in bzapi.get_all_player_ids()
        if bzapi.get_player_flag(pid)
        and bzapi.get_flag_cname(bzapi.get_player_flag(pid)) == flag.cname
    ]

def _can_kill(holder_id: int, victim_id: int, allow_team_kill: bool) -> bool:
    if holder_id == victim_id:
        return False
    if allow_team_kill:
        return True
    get_team = getattr(bzapi, "get_player_team", None)
    if not callable(get_team):
        return False
    th = get_team(holder_id)
    tv = get_team(victim_id)
    return th != -1 and tv != -1 and th != tv

def on_tick() -> None:
    now = time.time()
    radius = float(flag.get_parameter("radius") or 3.0)
    allow_team_kill = bool(flag.get_parameter("allow_team_kill") or False)
    interval = float(flag.get_parameter("interval") or 1.0)
    r2 = radius * radius

    holders = _holder_ids()
    if not holders:
        return

    positions: dict[int, tuple[float, float, float]] = {
        pid: bzapi.get_player_position(pid)
        for pid in bzapi.get_all_player_ids()
    }

    for holder in holders:
        last = _last_check.get(holder, 0.0)
        if now - last < interval:
            continue
        _last_check[holder] = now
        hpos = positions.get(holder)
        if not hpos:
            continue
        for pid, ppos in positions.items():
            if pid == holder:
                continue
            if _sqdist(hpos, ppos) <= r2:
                if _can_kill(holder, pid, allow_team_kill):
                    bzapi.kill_player(pid)

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    _last_check[player_id] = 0.0

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    _last_check.pop(player_id, None)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)
flag.add_callback(bzapi.event_type.TICK, on_tick)

flag.register()
