import bzapi
from .. import Flag

flag = Flag("forward_only")

flag.name = {
    "en": "Forward Only",
    "fr": "Avance Seulement",
    "de": "Nur Vorwärts",
    "es": "Solo Adelante",
    "pt": "Apenas Para Frente",
    "ja": "前進のみ",
    "ru": "Только вперед",
    "zh": "仅前进",
}

flag.description = {
    "en": "Disables reverse movement while holding this flag.",
    "fr": "Désactive la marche arrière pendant que ce drapeau est porté.",
    "de": "Deaktiviert Rückwärtsfahrt, solange diese Flagge gehalten wird.",
    "es": "Desactiva el movimiento hacia atrás mientras se lleva esta bandera.",
    "pt": "Desativa movimento para trás enquanto carrega esta bandeira.",
    "ja": "この旗を所持している間、後退を無効にします。",
    "ru": "Отключает движение назад, пока держат этот флаг.",
    "zh": "持有此旗帜时禁用倒退移动。",
}

flag.params = {}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        # 4-value tuple: (Forward, Reverse, Rotate Left, Rotate Right)
        bzapi.set_player_speed(player_id, (None, 0, None, None))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, 1.0)  # Reset to default

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
