import bzapi
from .. import Flag

flag = Flag("air_shot")

flag.name = {
    "en": "Air Shot",
    "fr": "Tir aérien",
    "de": "Luftschuss",
    "es": "Disparo aéreo",
    "pt": "Tiro aéreo",
    "ja": "エアショット",
    "ru": "Воздушный выстрел",
    "zh": "空中射击",
}

flag.description = {
    "en": "Fires multiple bullets with increasing vertical trajectories",
    "fr": "Tire plusieurs balles avec des trajectoires verticales de plus en plus élevées.",
    "de": "Feuert mehrere Kugeln mit zunehmend steilen vertikalen Flugbahnen.",
    "es": "Dispara varias balas con trayectorias verticales cada vez más altas.",
    "pt": "Dispara várias balas com trajetórias verticais cada vez más altas.",
    "ja": "垂直方向の軌道が徐々に高くなる複数の弾を発射します。",
    "ru": "Выпускает несколько пуль с постепенно возрастающими вертикальными траекториями.",
    "zh": "发射多枚子弹，垂直轨迹逐渐升高。",
}

flag.params = {}

flag.register()
