import bzapi
from .. import Flag

flag = Flag("narrow")

flag.name = {
    "en": "Narrow",
    "fr": "Étroit",
    "de": "Schmal",
    "es": "Estrecho",
    "pt": "Estreito",
    "ja": "ナロー",
    "ru": "Узкий",
    "zh": "狭窄的",
}
flag.description = {
    "en": "Reduces the width of the player holding this flag.",
    "fr": "Réduit la largeur du joueur portant ce drapeau.",
    "de": "Verringert die Breite des Spielers, der diesen Flag trägt.",
    "es": "Reduce el ancho del jugador que lleva este bandera.",
    "pt": "Reduz a largura do jogador que carrega esta bandeira.",
    "ja": "この旗を所持しているプレイヤーの幅を縮小します。",
    "ru": "Уменьшает ширину игрока, держащего этот флаг.",
    "zh": "减少持有此旗帜的玩家的宽度。"
}

flag.params = {
    'width_multiplier': 0.1,  # Multiplier for player width
}

# EVENT HANDLING

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        width_multiplier = flag.get_parameter('width_multiplier')
        bzapi.set_player_size(player_id, (None, width_multiplier, None))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_size(player_id, 1.0) # Reset to default size on drop


flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
