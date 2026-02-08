import time
import bzapi
from .. import Flag

flag = Flag("agility")

flag.name = {
    "en": "Agility",
    "fr": "Agilité",
    "de": "Agilität",
    "es": "Agilidad",
    "pt": "Agilidade",
    "ja": "敏捷性",
    "ru": "Ловкость",
    "zh": "敏捷",
}

flag.description = {
    "en": "Temporarily multiplies forward/reverse speed when activated; optional cooldown.",
    "fr": "Multiplie temporairement la vitesse avant/arrière lorsqu'elle est activée ; délai de récupération facultatif.",
    "de": "Multipliziert vorübergehend die Vorwärts-/Rückwärtsgeschwindigkeit bei Aktivierung; optionale Abklingzeit.",
    "es": "Multiplica temporalmente la velocidad hacia adelante/atrás al activarse; enfriamiento opcional.",
    "pt": "Multiplica temporariamente a velocidade para frente/trás ao ativar; tempo de recarga opcional.",
    "ja": "発動中、一時的に前進/後退速度を倍率で増やします。クールダウンは任意です。",
    "ru": "Временно умножает скорость вперед/назад при активации; необязательная перезарядка.",
    "zh": "激活时临时倍增前进/后退速度；可选冷却时间。",
}

flag.params = {
    "speed_multiplier": 3.0,
    "duration": 3.0,
    "cooldown": 0.0,
}


# Per-player state
_player_states: dict[int, dict] = {}  # player_id -> {'active':bool,'expires_at':float,'cooldown_until':float}

def _now() -> float:
    return time.time()

def _start_boost(player_id: int) -> None:
    params = flag.get_parameter("speed_multiplier"), \
             flag.get_parameter("duration"), \
             flag.get_parameter("cooldown")
    speed_multiplier, duration, cooldown = params
    if player_id not in _player_states:
        _player_states[player_id] = {'active': False, 'expires_at': 0.0, 'cooldown_until': 0.0}
    st = _player_states[player_id]
    now = _now()
    if st.get('active', False):
        st['expires_at'] = max(st['expires_at'], now + float(duration))
        return
    if now < st.get('cooldown_until', 0.0):
        return
    st['active'] = True
    st['expires_at'] = now + float(duration)
    st['cooldown'] = float(cooldown)
    bzapi.set_player_speed(player_id, (float(speed_multiplier), None))

def _end_boost(player_id: int) -> None:
    st = _player_states.get(player_id)
    if not st:
        return
    st['active'] = False
    now = _now()
    cooldown = st.get('cooldown', 0.0) or 0.0
    st['cooldown_until'] = now + float(cooldown)
    bzapi.set_player_speed(player_id, (1.0, None))

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    _start_boost(player_id)

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    _end_boost(player_id)
    # clear state to avoid leaks
    _player_states.pop(player_id, None)

def on_tick() -> None:
    now = _now()
    for player_id, st in list(_player_states.items()):
        if st.get('active') and now >= st.get('expires_at', 0.0):
            _end_boost(player_id)

# Optional helper for other plugins/host to retrigger the boost (e.g., on a client-side keypress)
def retrigger(player_id: int) -> None:
    """External callers can call this to re-trigger the agility boost for a player."""
    st = _player_states.get(player_id)
    now = _now()
    if st and (st.get('active') or now < st.get('cooldown_until', 0.0)):
        return
    _start_boost(player_id)


flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)
flag.add_callback(bzapi.event_type.TICK, on_tick)

flag.register()

