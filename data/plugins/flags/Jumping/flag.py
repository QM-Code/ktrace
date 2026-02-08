import bzapi
from .. import Flag

flag = Flag("jumping")

flag.name = {
    "en": "Jumping",
    "fr": "Saut",
    "de": "Springen",
    "es": "Salto",
    "pt": "Salto",
    "ja": "ジャンプ",
    "ru": "Прыжок",
    "zh": "跳跃",
}

flag.description = {}

flag.params = {}

flag.register()
