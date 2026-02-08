import bzapi
from .. import Flag

flag = Flag("no_jumping")

flag.name = {
    "en": "No Jumping",
    "fr": "Pas de saut",
    "de": "Kein Springen",
    "es": "Sin salto",
    "pt": "Sem salto",
    "ja": "ジャンプ禁止",
    "ru": "Без прыжков",
    "zh": "禁止跳跃",
}

flag.description = {}

flag.params = {}

flag.register()
