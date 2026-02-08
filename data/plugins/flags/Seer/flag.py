import bzapi
from .. import Flag

flag = Flag("seer")

flag.name = {
    "en": "Seer",
    "fr": "Voyant",
    "de": "Seher",
    "es": "Vidente",
    "pt": "Vidente",
    "ja": "予見者",
    "ru": "Провидец",
    "zh": "预言者",
}

flag.description = {}

flag.params = {}

flag.register()
