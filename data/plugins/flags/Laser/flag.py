import bzapi
from .. import Flag

flag = Flag("laser")

flag.name = {
    "en": "Laser",
    "fr": "Laser",
    "de": "Laser",
    "es": "Láser",
    "pt": "Laser",
    "ja": "レーザー",
    "ru": "Лазер",
    "zh": "激光",
}

flag.description = {
    "en": "Fires a piercing ray that travels instantly and persists for a short time.",
    "fr": "Tire un rayon perçant qui se propage instantanément et persiste un court instant.",
    "de": "Feuert einen durchdringenden Strahl, der sich sofort ausbreitet und kurz anhält.",
    "es": "Dispara un rayo penetrante que viaja al instante y persiste por un corto tiempo.",
    "pt": "Dispara um raio penetrante que viaja instantaneamente e persiste por um curto tempo.",
    "ja": "瞬時に届き短時間残る貫通レーザーを放ちます。",
    "ru": "Стреляет пробивающим лучом, который мгновенно проходит и кратко сохраняется.",
    "zh": "发射穿透光束，瞬间到达并短暂持续。",
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

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    bzapi.set_shot_defaults(player_id)

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
