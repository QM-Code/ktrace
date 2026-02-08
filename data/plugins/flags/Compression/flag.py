import bzapi
from .. import Flag

flag = Flag("compression")

flag.name = {
    "en": "Compression",
    "fr": "Compression",
    "de": "Kompression",
    "es": "Compresión",
    "pt": "Compressão",
    "ja": "圧縮",
    "ru": "Сжатие",
    "zh": "压缩",
}

flag.description = {
    "en": "Flattens the player holding this flag, making them harder to shoot but able to be run over",
    "fr": "Aplati le joueur portant ce drapeau, le rendant plus difficile à tirer mais pouvant être écrasé",
    "de": "Macht den Spieler, der diese Flagge hält, flach, wodurch er schwerer zu treffen ist, aber überfahren werden kann",
    "es": "Aplana al jugador que lleva esta bandera, haciéndolo más difícil de disparar pero capaz de ser atropellado",
    "pt": "Achata o jogador que carrega esta bandeira, tornando-o mais difícil de atirar, mas capaz de ser atropelado",
    "ja": "この旗を所持しているプレイヤーを平らにし、撃たれにくくしますが、轢かれる可能性があります",
    "ru": "Уплощает игрока, держащего этот флаг, делая его труднее для стрельбы, но позволяя быть перееханным",
    "zh": "压扁持有此旗帜的玩家，使其更难被射击，但可以被碾压",
}

flag.params = {
    'height_multiplier': 0.2
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        height_multiplier = flag.get_parameter('height_multiplier')
        bzapi.set_player_size(player_id, (None, None, height_multiplier))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_size(player_id, 1.0)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
