import bzapi
from .. import Flag

flag = Flag("high_speed")

flag.name = {
    "en": "High Speed",
    "fr": "Grande Vitesse",
    "de": "Hohe Geschwindigkeit",
    "es": "Alta Velocidad",
    "pt": "Alta Velocidade",
    "ja": "高速",
    "ru": "Высокая скорость",
    "zh": "高速",
}
flag.description = {
    "en": "Greatly increases the movement speed of the player holding this flag.",
    "fr": "Augmente fortement la vitesse de déplacement du joueur portant ce drapeau.",
    "de": "Erhöht die Bewegungsgeschwindigkeit des Spielers, der diese Flagge trägt, erheblich.",
    "es": "Aumenta en gran medida la velocidad de movimiento del jugador que lleva esta bandera.",
    "pt": "Aumenta bastante a velocidade de movimento do jogador que carrega esta bandeira.",
    "ja": "この旗を所持しているプレイヤーの移動速度を大幅に上げます。",
    "ru": "Сильно увеличивает скорость передвижения игрока, держащего этот флаг.",
    "zh": "大幅提高持有此旗帜的玩家的移动速度。",
}

flag.params = {
    "speed_multiplier": 2.0,  # Multiplier applied to player speed
}

# EVENT HANDLING

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        speed_multiplier = flag.get_parameter("speed_multiplier")
        bzapi.set_player_speed(player_id, (speed_multiplier, None))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, (1.0, None))  # Reset to default speed


flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
