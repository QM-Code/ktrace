import bzapi
from .. import Flag

flag = Flag("left_turn_only")

flag.name = {
    "en": "Left Turn Only",
    "fr": "Tourner à gauche seulement",
    "de": "Nur Linksdrehung",
    "es": "Solo Girar a la Izquierda",
    "pt": "Somente Virar à Esquerda",
    "ja": "左折のみ",
    "ru": "Только влево",
    "zh": "只向左转",
}

flag.description = {
    "en": "Disables right turning for the holder (right rotation = 0).",
    "fr": "Désactive le virage à droite pour le porteur (rotation droite = 0).",
    "de": "Deaktiviert das Rechts-Drehen für den Träger (Rechtsdrehung = 0).",
    "es": "Desactiva girar a la derecha para el portador (rotación derecha = 0).",
    "pt": "Desativa virar à direita para o portador (rotação direita = 0).",
    "ja": "所持者の右旋回を無効にします（右回転 = 0）。",
    "ru": "Отключает поворот вправо для носителя (вправо = 0).",
    "zh": "禁用持有者向右转（右旋转 = 0）。",
}

flag.params = {}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        # 4-value tuple: (Forward, Reverse, Rotate Left, Rotate Right)
        bzapi.set_player_speed(player_id, (None, None, None, 0))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, 1.0)  # reset to default

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
