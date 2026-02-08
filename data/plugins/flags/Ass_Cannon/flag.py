import bzapi
from .. import Flag

flag = Flag("ass_cannon")

flag.name = {
    "en": "Ass Cannon",
    "fr": "Canon arrière",
    "de": "Hinternkanone",
    "es": "Cañón trasero",
    "pt": "Canhão traseiro",
    "ja": "後方砲",
    "ru": "Задняя пушка",
    "zh": "后向炮",
}

flag.description = {
    "en": "Shoots one bullet forward and one bullet backward.",
    "fr": "Tire une balle vers l'avant et une balle vers l'arrière.",
    "de": "Feuert eine Kugel nach vorne und eine nach hinten.",
    "es": "Dispara una bala hacia adelante y una bala hacia atrás.",
    "pt": "Dispara uma bala para frente e uma bala para trás.",
    "ja": "前方に1発、後方に1発の弾を発射します。",
    "ru": "Стреляет одной пулей вперед и одной пулей назад.",
    "zh": "向前发射一发子弹，向后发射一发子弹。",
}

flag.params = {}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return

    bzapi.set_shot_count(player_id, 2)
    # Shot 0: forward (default, yaw=0)
    bzapi.set_shot_rotation(player_id, 0, (0.0, 0.0))
    # Shot 1: backward (yaw=pi)
    bzapi.set_shot_rotation(player_id, 1, (3.141592653589793, 0.0))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
