import bzapi
from .. import Flag

flag = Flag("guided_missile")

flag.name = {
    "en": "Guided Missile",
    "fr": "Missile guidé",
    "de": "Lenkwaffe",
    "es": "Misil guiado",
    "pt": "Míssil guiado",
    "ja": "誘導ミサイル",
    "ru": "Управляемая ракета",
    "zh": "制导导弹",
}

flag.description = {}

flag.params = {}

flag.register()
