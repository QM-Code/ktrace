import bzapi
from .. import Flag

flag = Flag("right_turn_only")

flag.name = {
    "en": "Right Turn Only",
    "fr": "Tourner à droite seulement",
    "de": "Nur Rechtsdrehung",
    "es": "Solo Girar a la Derecha",
    "pt": "Somente Virar à Direita",
    "ja": "右折のみ",
    "ru": "Только вправо",
    "zh": "只向右转",
}

flag.description = {
    "en": "Disables left turning for the holder (left rotation = 0).",
    "fr": "Désactive le virage à gauche pour le porteur (rotation gauche = 0).",
    "de": "Deaktiviert das Links-Drehen für den Träger (Linksdrehung = 0).",
    "es": "Desactiva girar a la izquierda para el portador (rotación izquierda = 0).",
    "pt": "Desativa virar à esquerda para o portador (rotação esquerda = 0).",
    "ja": "所持者の左旋回を無効にします（左回転 = 0）。",
    "ru": "Отключает поворот влево для носителя (влево = 0).",
    "zh": "禁用持有者向左转（左旋转 = 0）。",
}

flag.params = {}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, (None, None, 0, None))

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) == flag.cname:
        bzapi.set_player_speed(player_id, 1.0)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
