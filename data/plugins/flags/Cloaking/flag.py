import bzapi
from .. import Flag

flag = Flag("cloaking")

flag.name = {
    "en": "Cloaking",
    "fr": "Camouflage",
    "de": "Tarnung",
    "es": "Ocultamiento",
    "pt": "Ocultação",
    "ja": "クローク",
    "ru": "Маскировка",
    "zh": "隐形",
}

flag.description = {
    "en": "Makes the player almost-invisible and resistant to ray effects.",
    "fr": "Rend le joueur presque invisible et résistant aux effets de rayon.",
    "de": "Macht den Spieler fast unsichtbar und widerstandsfähig gegen Strahleffekte.",
    "es": "Hace que el jugador sea casi invisible y resistente a los efectos de rayos.",
    "pt": "Torna o jogador quase invisível e resistente a efeitos de raios.",
    "ja": "プレイヤーをほぼ透明にし、レイ効果に耐性を持たせます。",
    "ru": "Делает игрока почти невидимым и устойчивым к лучевым эффектам.",
    "zh": "使玩家几乎隐形并对射线效果具有抗性。",
}

flag.params = {
    "alpha": 0.95,
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        alpha = flag.get_parameter("alpha")
        bzapi.set_player_color(player_id, (None, None, None, alpha))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_color(player_id, (None, None, None, 1.0))

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
