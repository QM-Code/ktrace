import math
import bzapi
from .. import Flag

flag = Flag("shock_chain")

flag.name = {
    "en": "Shock Chain",
    "fr": "Chaîne de choc",
    "de": "Schockkette",
    "es": "Cadena de choque",
    "pt": "Cadeia de choque",
    "ja": "ショックチェーン",
    "ru": "Цепь шока",
    "zh": "冲击链",
}

flag.description = {
    "en": "Fires a series of shockwaves in a straight line, one after another.",
    "fr": "Tire une série d'ondes de choc en ligne droite, l'une après l'autre.",
    "de": "Feuert eine Reihe von Schockwellen in gerader Linie, eine nach der anderen.",
    "es": "Dispara una serie de ondas de choque en línea recta, una tras otra.",
    "pt": "Dispara uma série de ondas de choque em linha reta, uma após a outra.",
    "ja": "直線上に連続する衝撃波を次々に発射します。",
    "ru": "Выпускает серию ударных волн по прямой, одну за другой.",
    "zh": "沿直线依次发射一系列冲击波。",
}

flag.params = {
    "wave_count": 5,         # Number of shockwaves
    "spacing": 6.0,          # Distance between each wave (units)
    "delay": 0.15,           # Delay between each wave (seconds)
    "distance": 30.0,        # How far each wave travels
    "longevity": 0.5,        # How long each wave persists
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return

    wave_count = int(flag.get_parameter("wave_count"))
    spacing = float(flag.get_parameter("spacing"))
    delay = float(flag.get_parameter("delay"))
    distance = float(flag.get_parameter("distance"))
    longevity = float(flag.get_parameter("longevity"))

    bzapi.set_shot_count(player_id, wave_count)

    for i in range(wave_count):
        # All waves go forward (yaw=0, pitch=0)
        bzapi.set_shot_type(player_id, i, "Wave")
        bzapi.set_shot_rotation(player_id, i, (0.0, 0.0))
        bzapi.set_shot_distance(player_id, distance)
        bzapi.set_shot_longevity(player_id, longevity)
        bzapi.set_shot_delay(player_id, i, i * delay)
        # Each wave starts further along the line
        bzapi.set_shot_position(player_id, i, (i * spacing, 0.0, 0.0))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
