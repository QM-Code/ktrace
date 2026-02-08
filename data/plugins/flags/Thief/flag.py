import bzapi
from .. import Flag

flag = Flag("thief")

flag.name = {
    "en": "Thief",
    "fr": "Voleur",
    "de": "Dieb",
    "es": "Ladrón",
    "pt": "Ladrão",
    "ja": "盗賊",
    "ru": "Вор",
    "zh": "小偷",
}

flag.description = {
    "en": "Fires a ray that steals a flag from the first player it hits.",
    "fr": "Tire un rayon qui vole un drapeau au premier joueur qu'il touche.",
    "de": "Feuert einen Strahl, der dem ersten getroffenen Spieler eine Flagge stiehlt.",
    "es": "Dispara un rayo que roba una bandera del primer jugador al que impacta.",
    "pt": "Dispara um raio que rouba uma bandeira do primeiro jogador que atinge.",
    "ja": "最初に当たったプレイヤーから旗を奪う光線を発射します。",
    "ru": "Стреляет лучом, который крадет флаг у первого пораженного игрока.",
    "zh": "发射一束光线，夺走首个命中的玩家的旗帜。",
}

flag.params = {
    "distance": 100.0,    # units, how far the ray travels
    "longevity": 0.2,     # seconds, how long the ray persists
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return

    distance = float(flag.get_parameter("distance"))
    longevity = float(flag.get_parameter("longevity"))

    bzapi.set_shot_type(player_id, 0, "Ray")
    bzapi.set_shot_distance(player_id, distance)
    bzapi.set_shot_longevity(player_id, longevity)

def on_player_hit(victim_id: int, killer_id: int, weapon_id: int, flags: int) -> bool:
    # Only act if killer has Thief flag and victim has a flag (not Thief)
    killer_flag = bzapi.get_player_flag(killer_id)
    victim_flag = bzapi.get_player_flag(victim_id)
    if not killer_flag or not victim_flag:
        return False
    if bzapi.get_flag_cname(killer_flag) != flag.cname:
        return False
    if bzapi.get_flag_cname(victim_flag) == flag.cname:
        return False

    # Steal the flag
    bzapi.drop_player_flag(victim_id)
    bzapi.give_player_flag(killer_id, bzapi.get_flag_cname(victim_flag))
    bzapi.drop_player_flag(killer_id)  # Drop Thief flag after successful steal
    return True  # Prevent default death behavior

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)
flag.add_callback(bzapi.event_type.PLAYER_HIT, on_player_hit)

flag.register()
