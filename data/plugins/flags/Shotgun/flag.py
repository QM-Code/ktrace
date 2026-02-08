import random
import bzapi
from .. import Flag

flag = Flag("shotgun")

flag.name = {
    "en": "Shotgun",
    "fr": "Fusil à pompe",
    "de": "Schrotflinte",
    "es": "Escopeta",
    "pt": "Espingarda",
    "ja": "ショットガン",
    "ru": "Дробовик",
    "zh": "霰弹枪",
}

flag.description = {
    "en": "Fires a spread of pellets in a cone pattern with staggered timing.",
    "fr": "Tire une gerbe de plombs en cône avec un délai échelonné.",
    "de": "Feuert eine Streuung von Pellets in einem Kegelmuster mit gestaffelter Zeit.",
    "es": "Dispara una dispersión de perdigones en un patrón cónico con tiempos escalonados.",
    "pt": "Dispara uma dispersão de chumbos em padrão de cone com tempo escalonado.",
    "ja": "円錐状に散弾を発射し、発射タイミングが少しずつずれます。",
    "ru": "Выпускает разброс дробин конусом с небольшими задержками.",
    "zh": "以锥形散射发射多颗弹丸，并带有错开的时间。",
}

flag.params = {
    "pellet_count": 8,
    "spread_angle": 0.15,  # radians, max deviation from center
    "max_delay": 0.05,     # seconds, max stagger between pellets
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    pellet_count = int(flag.get_parameter("pellet_count"))
    spread_angle = float(flag.get_parameter("spread_angle"))
    max_delay = float(flag.get_parameter("max_delay"))
    bzapi.set_shot_count(player_id, pellet_count)
    
    for i in range(pellet_count):
        # Random angle in both directions
        angle_yaw = random.uniform(-spread_angle, spread_angle)
        angle_pitch = random.uniform(-spread_angle, spread_angle)
        bzapi.set_shot_rotation(player_id, i, (angle_yaw, angle_pitch))
        # Random delay for staggered firing
        delay = random.uniform(0, max_delay)
        bzapi.set_shot_delay(player_id, i, delay)

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
