import math
import bzapi
from .. import Flag

flag = Flag("death_blossom")

flag.name = {
    "en": "Death Blossom",
    "fr": "Fleur de mort",
    "de": "Todesblüte",
    "es": "Flor mortal",
    "pt": "Flor da morte",
    "ja": "デスブロッサム",
    "ru": "Цветок смерти",
    "zh": "死亡绽放",
}

flag.description = {
    "en": "Fires a ring of bullets in all directions from the player.",
    "fr": "Tire un anneau de balles dans toutes les directions depuis le joueur.",
    "de": "Feuert einen Kugelring in alle Richtungen vom Spieler aus.",
    "es": "Dispara un anillo de balas en todas direcciones desde el jugador.",
    "pt": "Dispara um anel de balas em todas as direções a partir do jogador.",
    "ja": "プレイヤーから全方向に弾の輪を発射します。",
    "ru": "Выпускает кольцо пуль во все стороны от игрока.",
    "zh": "从玩家处向所有方向发射一圈子弹。",
}

flag.params = {
    "bullet_count": 12,   # Number of bullets in the ring
    "longevity": 1.0,     # seconds, how long the bullets persist
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return

    bullet_count = int(flag.get_parameter("bullet_count"))
    longevity = float(flag.get_parameter("longevity"))

    bzapi.set_shot_count(player_id, bullet_count)
    angle_step = 2 * math.pi / bullet_count

    for i in range(bullet_count):
        angle = i * angle_step
        # Set yaw to angle, pitch to 0 (horizontal plane)
        bzapi.set_shot_rotation(player_id, i, (angle, 0.0))
        bzapi.set_shot_longevity(player_id, longevity)

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
