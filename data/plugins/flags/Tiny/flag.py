import bzapi
from .. import Flag

flag = Flag("tiny")

flag.name = {
    "en": "Tiny",
    "fr": "Minuscule",
    "de": "Winzig",
    "es": "Diminuto",
    "pt": "Minúsculo",
    "ja": "小さな",
    "ru": "Крошечный",
    "zh": "微小的",
}
flag.description = {
    "en": "Reduces size and increases speed of the player holding this flag.",
    "fr": "Réduit la taille et augmente la vitesse du joueur portant ce drapeau.",
    "de": "Verringert die Größe und erhöht die Geschwindigkeit des Spielers, der diesen Flag trägt.",
    "es": "Reduce el tamaño y aumenta la velocidad del jugador que lleva este bandera.",
    "pt": "Reduz o tamanho e aumenta a velocidade do jogador que carrega esta bandeira.",
    "ja": "この旗を所持しているプレイヤーのサイズを縮小し、速度を上げます。",
    "ru": "Уменьшает размер и увеличивает скорость игрока, держащего этот флаг.",
    "zh": "减少持有此旗帜的玩家的尺寸并增加其速度。"
}

flag.params = {
    'speed_multiplier': 1.2,  # Multiplier for player speed
    'size_multiplier': 0.5,   # Multiplier for player size
}

# EVENT HANDLING

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        speed_multiplier = flag.get_parameter('speed_multiplier')
        size_multiplier = flag.get_parameter('size_multiplier')
        bzapi.set_player_speed(player_id, (speed_multiplier, None))
        bzapi.set_player_size(player_id, (size_multiplier, None))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, (1.0, None))
        bzapi.set_player_size(player_id, (1.0, None)) # Reset to default size on drop


flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
