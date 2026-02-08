import bzapi
from .. import Flag

flag = Flag("annoying_cat")

flag.name = {
    "en": "Annoying Cat",
    "fr": "Chat agaçant",
    "de": "Lästige Katze",
    "es": "Gato molesto",
    "pt": "Gato irritante",
    "ja": "うるさい猫",
    "ru": "Назойливый кот",
    "zh": "讨厌的猫",
}

flag.description = {
    "en": "Complain to everyone about random things in the game.",
    "fr": "Se plaint à tout le monde de choses aléatoires dans le jeu.",
    "de": "Beschwert sich bei allen über zufällige Dinge im Spiel.",
    "es": "Se queja con todos sobre cosas aleatorias en el juego.",
    "pt": "Reclama para todos sobre coisas aleatórias no jogo.",
    "ja": "ゲーム内のあれこれをみんなに文句を言います。",
    "ru": "Жалуется всем на случайные вещи в игре.",
    "zh": "向所有人抱怨游戏里的随机事情。",
}

flag.params = {}

flag.register()
