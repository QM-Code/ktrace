import bzapi
from .. import Flag

flag = Flag("reverse_controls")

flag.name = {
    "en": "Reverse Controls",
    "fr": "Contrôles inversés",
    "de": "Umgekehrte Steuerung",
    "es": "Controles invertidos",
    "pt": "Controles invertidos",
    "ja": "操作反転",
    "ru": "Обратное управление",
    "zh": "反向控制",
}

flag.description = {
    "en": "Reverses forward/backward and left/right controls.",
    "fr": "Inverse les commandes avant/arrière et gauche/droite.",
    "de": "Kehrt die Vorwärts-/Rückwärts- und Links-/Rechts-Steuerung um.",
    "es": "Invierte los controles de adelante/atrás e izquierda/derecha.",
    "pt": "Inverte os controles de frente/trás e esquerda/direita.",
    "ja": "前後と左右の操作を反転します。",
    "ru": "Инвертирует управление вперед/назад и влево/вправо.",
    "zh": "反转前进/后退与左/右控制。",
}

flag.params = {}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        # 4-value tuple: (Forward, Reverse, Rotate Left, Rotate Right)
        bzapi.set_player_speed(player_id, (0, 1.0, 0, 1.0))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, 1.0)  # Reset to default

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
