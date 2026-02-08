import bzapi
from .. import Flag

flag = Flag("quick_turn")

flag.name = {
    "en": "Quick Turn",
    "fr": "Virage Rapide",
    "de": "Schnelle Wendung",
    "es": "Giro Rápido",
    "pt": "Virada Rápida",
    "ja": "クイックターン",
    "ru": "Быстрый Поворот",
    "zh": "快速转向",
}
flag.description = {
    "en": "Increases the tank's angular rotation speed while holding this flag.",
    "fr": "Augmente la vitesse de rotation angulaire du char en portant ce drapeau.",
    "de": "Erhöht die Winkelgeschwindigkeit des Panzers, solange diese Flagge gehalten wird.",
    "es": "Aumenta la velocidad de rotación angular del tanque mientras lleva esta bandera.",
    "pt": "Aumenta a velocidade de rotação angular do tanque enquanto carrega esta bandeira.",
    "ja": "この旗を所持している間、戦車の角速度を増加させます。",
    "ru": "Увеличивает угловую скорость поворота танка, пока игрок держит этот флаг.",
    "zh": "持有此旗帜时增加坦克的角旋转速度。",
}

flag.params = {
    "rotation_multiplier": 1.5,  # Multiplier for angular rotation speed
}

# EVENT HANDLING

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        rotation_multiplier = flag.get_parameter("rotation_multiplier")
        bzapi.set_player_speed(player_id, (None, rotation_multiplier))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, (None, 1.0))  # Reset rotation multiplier to default


flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
