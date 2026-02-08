import bzapi
from .. import Flag

flag = Flag("momentum")

flag.name = {
    "en": "Momentum",
    "fr": "Momentum",
    "de": "Impuls",
    "es": "Momento",
    "pt": "Momento",
    "ja": "モメンタム",
    "ru": "Импульс",
    "zh": "动量",
}

flag.description = {
    "en": "Increases the mass of the player holding this flag.",
    "fr": "Augmente la masse du joueur portant ce drapeau.",
    "de": "Erhöht die Masse des Spielers, der diese Flagge trägt.",
    "es": "Aumenta la masa del jugador que lleva esta bandera.",
    "pt": "Aumenta a massa do jogador que carrega esta bandeira.",
    "ja": "この旗を所持しているプレイヤーの質量を増加させます。",
    "ru": "Увеличивает массу игрока, держащего этот флаг.",
    "zh": "增加持有此旗帜的玩家的质量。",
}

flag.params = {
    "mass_multiplier": 1.5,  # Multiplier for mass
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        mass_mul = flag.get_parameter("mass_multiplier")
        bzapi.set_player_size(player_id, (None, float(mass_mul)))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_size(player_id, (None, 1.0))  # Reset mass to default

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
