import math
import bzapi
from .. import Flag

flag = Flag("nova")

flag.name = {
    "en": "Nova",
    "fr": "Nova",
    "de": "Nova",
    "es": "Nova",
    "pt": "Nova",
    "ja": "ノヴァ",
    "ru": "Нова",
    "zh": "新星",
}

flag.description = {
    "en": "Fires a burst of bullets in a semi-spherical pattern above the player.",
    "fr": "Tire une rafale de balles en motif semi-sphérique au-dessus du joueur.",
    "de": "Feuert eine Salve von Kugeln in einem halbkugelförmigen Muster über dem Spieler.",
    "es": "Dispara una ráfaga de balas en un patrón semiesférico sobre el jugador.",
    "pt": "Dispara uma rajada de balas em um padrão semiesférico acima do jogador.",
    "ja": "プレイヤーの上に半球状のパターンで弾のバーストを放ちます。",
    "ru": "Выпускает очередь пуль по полусферической схеме над игроком.",
    "zh": "在玩家上方以半球形模式发射一阵子弹。",
}

flag.params = {
    "bullet_count": 20,   # Number of bullets in the burst
    "longevity": 1.0,     # seconds, how long the bullets persist
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return

    bullet_count = int(flag.get_parameter("bullet_count"))
    longevity = float(flag.get_parameter("longevity"))

    bzapi.set_shot_count(player_id, bullet_count)

    # Distribute bullets over a hemisphere (pitch: 0 to pi/2, yaw: 0 to 2pi)
    for i in range(bullet_count):
        # Golden Section Spiral for even distribution on a hemisphere
        t = float(i) / bullet_count
        yaw = 2 * math.pi * t
        pitch = math.acos(1 - t) / 2  # restrict pitch to [0, pi/2]
        bzapi.set_shot_rotation(player_id, i, (yaw, pitch))
        bzapi.set_shot_longevity(player_id, longevity)

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
