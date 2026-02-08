import bzapi
from .. import Flag

flag = Flag("triple_point")

flag.name = {
    "en": "Triple Point",
    "fr": "Triple point",
    "de": "Dreifachpunkt",
    "es": "Triple punto",
    "pt": "Triplo ponto",
    "ja": "トリプルポイント",
    "ru": "Тройная точка",
    "zh": "三点",
}

flag.description = {}

flag.params = {}

flag.register()
