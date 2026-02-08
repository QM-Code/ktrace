import bzapi
from .. import Flag

flag = Flag("dimension_door")

flag.name = {
    "en": "Dimension Door",
    "fr": "Porte dimensionnelle",
    "de": "Dimensionsportal",
    "es": "Puerta dimensional",
    "pt": "Porta dimensional",
    "ja": "ディメンジョンドア",
    "ru": "Дверь измерений",
    "zh": "次元之门",
}

flag.description = {
    "en": "Allows the tank to teleport.",
    "fr": "Permet au char de se téléporter.",
    "de": "Erlaubt dem Panzer zu teleportieren.",
    "es": "Permite que el tanque se teletransporte.",
    "pt": "Permite que o tanque se teletransporte.",
    "ja": "戦車がテレポートできるようになります。",
    "ru": "Позволяет танку телепортироваться.",
    "zh": "允许坦克进行传送。",
}

flag.params = {}

flag.register()
