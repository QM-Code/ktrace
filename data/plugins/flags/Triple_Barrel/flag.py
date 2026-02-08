
import bzapi
from .. import Flag

flag = Flag("triple_barrel")

flag.name = {
    "en": "Triple Barrel",
    "fr": "Triple canon",
    "de": "Dreifachlauf",
    "es": "Triple cañón",
    "pt": "Canhão triplo",
    "ja": "トリプルバレル",
    "ru": "Тройной ствол",
    "zh": "三管",
}

flag.description = {
    "en": "Tank shoots a spray of three shots (two extra side shots).",
    "fr": "Le char tire une rafale de trois coups (deux tirs latéraux supplémentaires).",
    "de": "Der Panzer schießt eine Salve aus drei Schüssen (zwei zusätzliche Seitenschüsse).",
    "es": "El tanque dispara una ráfaga de tres tiros (dos tiros laterales extra).",
    "pt": "O tanque dispara uma rajada de três tiros (dois tiros laterais extras).",
    "ja": "戦車が3発の散弾を発射します（左右に追加の2発）。",
    "ru": "Танк стреляет залпом из трех выстрелов (две дополнительные боковые пули).",
    "zh": "坦克发射三连弹（左右各一发额外子弹）。",
}

flag.params = {
    "angle": 0.12,  # radians offset for side shots
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    angle = float(flag.get_parameter("angle"))
    bzapi.set_shot_count(player_id, 3)
    bzapi.set_shot_rotation(player_id, 1, (-angle, 0.0))
    bzapi.set_shot_rotation(player_id, 2, (angle, 0.0))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
