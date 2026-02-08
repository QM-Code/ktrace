import random
import math
import bzapi
from .. import Flag

flag = Flag("skyfire")

flag.name = {
    "en": "Skyfire",
    "fr": "Feu du ciel",
    "de": "Himmelsfeuer",
    "es": "Fuego del cielo",
    "pt": "Fogo do céu",
    "ja": "スカイファイア",
    "ru": "Небесный огонь",
    "zh": "天火",
}

flag.description = {
    "en": "Rains shots from above in a spread, with each shot slightly delayed and influenced by tank movement.",
    "fr": "Fait pleuvoir des tirs depuis le ciel en dispersion, chaque tir étant légèrement retardé et influencé par le mouvement du char.",
    "de": "Lässt Schüsse von oben in einer Streuung niedergehen, wobei jeder Schuss leicht verzögert und von der Panzerbewegung beeinflusst wird.",
    "es": "Hace llover disparos desde arriba en dispersión, con cada disparo ligeramente retrasado e influido por el movimiento del tanque.",
    "pt": "Faz chover tiros de cima em dispersão, com cada tiro ligeiramente atrasado e influenciado pelo movimento do tanque.",
    "ja": "上空から散弾状に弾を降らせ、各弾はわずかに遅れて発射され、戦車の動きの影響を受けます。",
    "ru": "Обрушивает сверху рассеянные выстрелы, каждый выстрел слегка задержан и зависит от движения танка.",
    "zh": "从上方散射落下子弹，每发子弹略有延迟并受坦克移动影响。",
}

flag.params = {
    "shot_count": 20,
    "height": 20.0,         # units above the player
    "radius": 8.0,          # spread radius
    "speed": 30.0,          # downward speed
    "max_delay": 0.2,       # seconds, max stagger between shots
    "velocity_influence": 0.2,  # fraction of player velocity to add to each shot
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return

    shot_count = int(flag.get_parameter("shot_count"))
    height = float(flag.get_parameter("height"))
    radius = float(flag.get_parameter("radius"))
    speed = float(flag.get_parameter("speed"))
    max_delay = float(flag.get_parameter("max_delay"))
    velocity_influence = float(flag.get_parameter("velocity_influence"))

    bzapi.set_shot_count(player_id, shot_count)

    player_pos = bzapi.get_player_position(player_id)
    player_vel = bzapi.get_player_velocity(player_id)

    for i in range(shot_count):
        # Random position in a circle above the player
        angle = random.uniform(0, 2 * 3.141592653589793)
        r = random.uniform(0, radius)
        offset_x = r * math.cos(angle)
        offset_y = r * math.sin(angle)
        shot_pos = (player_pos[0] + offset_x, player_pos[1] + offset_y, player_pos[2] + height)
        bzapi.set_shot_position(player_id, i, shot_pos)

        # Downward trajectory, with a bit of player movement influence
        vx = player_vel[0] * velocity_influence
        vy = player_vel[1] * velocity_influence
        vz = -abs(speed)
        bzapi.set_shot_velocity(player_id, i, (vx, vy, vz))

        # Small random delay for each shot
        delay = random.uniform(0, max_delay)
        bzapi.set_shot_delay(player_id, i, delay)

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
