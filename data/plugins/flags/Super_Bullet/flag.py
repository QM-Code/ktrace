import bzapi
from .. import Flag

flag = Flag("super_bullet")

flag.name = {
    "en": "Super Bullet",
    "fr": "Super balle",
    "de": "Superkugel",
    "es": "Super bala",
    "pt": "Super bala",
    "ja": "スーパーバレット",
    "ru": "Суперпуля",
    "zh": "超级子弹",
}

flag.description = {}

flag.params = {}

flag.register()
