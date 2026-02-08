import bzapi
from .. import Flag

flag = Flag("machine_gun")

flag.name = {
    "en": "Machine Gun",
    "fr": "Mitrailleuse",
    "de": "Maschinengewehr",
    "es": "Ametralladora",
    "pt": "Metralhadora",
    "ja": "マシンガン",
    "ru": "Пулемет",
    "zh": "机枪",
}

flag.description = {
    "en": "Allows continuous firing by holding down the fire key.",
    "fr": "Permet le tir continu en maintenant la touche de tir.",
    "de": "Ermöglicht Dauerfeuer, wenn die Feuertaste gedrückt gehalten wird.",
    "es": "Permite disparo continuo manteniendo pulsada la tecla de fuego.",
    "pt": "Permite disparo contínuo mantendo a tecla de disparo pressionada.",
    "ja": "発射キーを押し続けることで連続射撃が可能になります。",
    "ru": "Позволяет вести непрерывный огонь при удержании кнопки выстрела.",
    "zh": "按住开火键即可连续射击。",
}

flag.params = {
    "reload_time": 0.1,
    "magazine_size": 30,
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return

    reload_time = float(flag.get_parameter("reload_time"))
    magazine_size = int(flag.get_parameter("magazine_size"))

    bzapi.set_shot_magazine_size(player_id, magazine_size)
    bzapi.set_shot_reload_time(player_id, reload_time)
    bzapi.set_shot_autofire(player_id, True)

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
