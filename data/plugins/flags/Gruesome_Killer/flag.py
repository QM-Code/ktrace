import bzapi
from .. import Flag

flag = Flag("gruesome_killer")

flag.name = {
    "en": "Gruesome Killer",
    "fr": "Tueur atroce",
    "de": "Grausamer Killer",
    "es": "Asesino brutal",
    "pt": "Assassino cruel",
    "ja": "残忍な殺し屋",
    "ru": "Жестокий убийца",
    "zh": "残忍杀手",
}

flag.description = {}

flag.params = {}

flag.register()
