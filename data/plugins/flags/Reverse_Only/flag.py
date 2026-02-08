import bzapi
from .. import Flag

flag = Flag("reverse_only")

flag.name = {
    "en": "Reverse Only",
    "fr": "Retraite Seulement",
    "de": "Nur Rückwärts",
    "es": "Solo Reversa",
    "pt": "Apenas Ré",
    "ja": "後退のみ",
    "ru": "Только назад",
    "zh": "仅后退",
}

flag.description = {
    "en": "Disables forward movement while holding this flag.",
    "fr": "Désactive l'avancement pendant que ce drapeau est porté.",
    "de": "Deaktiviert Vorwärtsfahrt, solange diese Flagge gehalten wird.",
    "es": "Desactiva el movimiento hacia adelante mientras se lleva esta bandera.",
    "pt": "Desativa movimento para frente enquanto carrega esta bandeira.",
    "ja": "この旗を所持している間、前進を無効にします。",
    "ru": "Отключает движение вперед, пока держат этот флаг.",
    "zh": "持有此旗帜时禁用前进移动。",
}

flag.params = {}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        # 4-value tuple: (Forward, Reverse, Rotate Left, Rotate Right)
        bzapi.set_player_speed(player_id, (0, None, None, None))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, 1.0)  # Reset to default

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
