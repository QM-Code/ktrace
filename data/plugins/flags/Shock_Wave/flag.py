import bzapi
from .. import Flag

flag = Flag("shock_wave")

flag.name = {
    "en": "Shock Wave",
    "fr": "Onde de Choc",
    "de": "Schockwelle",
    "es": "Onda de Choque",
    "pt": "Onda de Choque",
    "ja": "ショックウェーブ",
    "ru": "Ударная Волна",
    "zh": "冲击波",
}

flag.description = {
    "en": "Fires a spherical force wave radiating out from the player.",
    "fr": "Tire une onde de force sphérique rayonnant à partir du joueur.",
    "de": "Feuert eine kugelförmige Kraftwelle aus, die vom Spieler ausgeht.",
    "es": "Dispara una onda de fuerza esférica que irradia desde el jugador.",
    "pt": "Dispara uma onda de força esférica irradiando do jogador.",
    "ja": "プレイヤーから放射状に広がる球状の力の波を発射します。",
    "ru": "Выпускает сферическую ударную волну, исходящую от игрока.",
    "zh": "从玩家发射出辐射的球形力波。",
}

flag.params = {
    "distance": 30.0,    # units, how far the wave travels
    "longevity": 0.5,    # seconds, how long the wave persists
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return

    distance = float(bzapi.get_flag_parameter(flag.cname, "distance"))
    longevity = float(bzapi.get_flag_parameter(flag.cname, "longevity"))

    bzapi.set_shot_type(player_id, 0, "Wave")
    bzapi.set_shot_distance(player_id, distance)
    bzapi.set_shot_longevity(player_id, longevity)

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
