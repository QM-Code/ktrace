import bzapi
from .. import Flag

flag = Flag("shield")

flag.name = {
    "en": "Shield",
    "fr": "Bouclier",
    "de": "Schild",
    "es": "Escudo",
    "pt": "Escudo",
    "ja": "シールド",
    "ru": "Щит",
    "zh": "护盾",
}

flag.description = {
    "en": "Absorbs one hit: the flag is dropped and the holder survives that hit.",
    "fr": "Absorbe un coup : le drapeau est lâché et le porteur survit à ce coup.",
    "de": "Absorbiert einen Treffer: Die Flagge wird fallen gelassen und der Träger überlebt den Treffer.",
    "es": "Absorbe un golpe: se suelta la bandera y el portador sobrevive a ese golpe.",
    "pt": "Absorve um golpe: a bandeira é largada e o portador sobrevive a esse golpe.",
    "ja": "1回分の被弾を吸収します。旗は落ち、所持者はその一撃に耐えます。",
    "ru": "Поглощает один удар: флаг сбрасывается, а носитель переживает этот удар.",
    "zh": "吸收一次命中：旗帜掉落，持有者存活该次命中。",
}

flag.params = {
    "drop_if_hit": True,
}

def on_player_hit(victim_id: int, killer_id: int, weapon_id: int, flags: int) -> bool:
    try:
        flag_id = bzapi.get_player_flag(victim_id)
    except Exception:
        return False

    if not flag_id:
        return False

    try:
        ns = bzapi.get_flag_cname(flag_id)
    except Exception:
        return False

    if ns != flag.cname:
        return False

    try:
        if flag.get_parameter("drop_if_hit"):
            bzapi.drop_player_flag(victim_id)
    except Exception:
        # best-effort drop; ignore errors
        try:
            bzapi.drop_player_flag(victim_id)
        except Exception:
            pass

    # Returning True signals the event was handled and prevents default death behavior.
    return True

flag.add_callback(bzapi.event_type.PLAYER_HIT, on_player_hit)

flag.register()
