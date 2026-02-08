import bzapi
from .. import Flag

flag = Flag("obesity")

flag.name = {
    "en": "Obesity",
    "fr": "Obésité",
    "de": "Fettleibigkeit",
    "es": "Obesidad",
    "pt": "Obesidade",
    "ja": "肥満",
    "ru": "Ожирение",
    "zh": "肥胖",
}

flag.description = {
    "en": "Increases overall size and mass of the player holding this flag.",
    "fr": "Augmente la taille et la masse du joueur portant ce drapeau.",
    "de": "Erhöht die Gesamtgröße und Masse des Spielers, der diese Flagge trägt.",
    "es": "Aumenta el tamaño y la masa del jugador que lleva esta bandera.",
    "pt": "Aumenta o tamanho e a massa do jogador que carrega esta bandeira.",
    "ja": "この旗を所持しているプレイヤーの全体サイズと質量を増加させます。",
    "ru": "Увеличивает общий размер и массу игрока, держащего этот флаг.",
    "zh": "增加持有此旗帜的玩家的整体尺寸和质量。",
}

flag.params = {
    "size_multiplier": 1.75,  # Overall size multiplier
    "mass_multiplier": 1.5,   # Mass multiplier
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        size_mul = flag.get_parameter("size_multiplier")
        mass_mul = flag.get_parameter("mass_multiplier")
        bzapi.set_player_size(player_id, (float(size_mul), float(mass_mul)))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_size(player_id, (1.0, 1.0))  # Reset size and mass to defaults

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
