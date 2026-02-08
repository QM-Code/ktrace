import bzapi
from .. import Flag

flag = Flag("invisible_bullet")

flag.name = {
    "en": "Invisible Bullet",
    "fr": "Balle invisible",
    "de": "Unsichtbare Kugel",
    "es": "Bala invisible",
    "pt": "Bala invisível",
    "ja": "透明な弾",
    "ru": "Невидимая пуля",
    "zh": "隐形子弹",
}

flag.description = {}

flag.params = {}

flag.register()
