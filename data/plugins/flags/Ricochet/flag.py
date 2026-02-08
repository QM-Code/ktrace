import bzapi
from .. import Flag

flag = Flag("ricochet")

flag.name = {
    "en": "Ricochet",
    "fr": "Ricochet",
    "de": "Rikoschett",
    "es": "Rebote",
    "pt": "Ricochete",
    "ja": "リコシェ",
    "ru": "Рикошет",
    "zh": "跳弹",
}

flag.description = {
    "en": "Standard shots (but not waves) will bounce off world geometry while you hold this flag.",
    "fr": "Les tirs standards (mais pas les ondes) rebondissent sur la géométrie du monde tant que vous portez ce drapeau.",
    "de": "Standardschüsse (aber keine Wellen) prallen von der Weltgeometrie ab, solange du diese Flagge hältst.",
    "es": "Los disparos estándar (pero no las ondas) rebotan en la geometría del mundo mientras llevas esta bandera.",
    "pt": "Os tiros padrão (mas não as ondas) ricocheteiam na geometria do mundo enquanto você carrega esta bandeira.",
    "ja": "この旗を所持している間、通常弾（波動は除く）が地形に跳ね返ります。",
    "ru": "Обычные выстрелы (но не волны) будут рикошетить от геометрии мира, пока вы держите этот флаг.",
    "zh": "持有此旗帜时，普通子弹（非波动）会在地图几何体上反弹。",
}

flag.params = {
    "enabled_by_default": True,  # unused here but available for world overrides
}

def on_pickup(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    try:
        bzapi.set_player_ricochet(player_id, True)
    except Exception:
        pass

def on_drop(player_id: int, flag_id: int) -> None:
    if bzapi.get_flag_cname(flag_id) != flag.cname:
        return
    try:
        bzapi.set_player_ricochet(player_id, False)
    except Exception:
        pass

# Ensure ricochet is cleared when plugin loads (no-op if not needed)
def _cleanup() -> None:
    for pid in bzapi.get_all_player_ids():
        try:
            if bzapi.get_player_flag(pid) and bzapi.get_flag_cname(bzapi.get_player_flag(pid)) != flag.cname:
                continue
            # If the player currently holds this flag, enable; otherwise ensure disabled.
            if bzapi.get_player_flag(pid) and bzapi.get_flag_cname(bzapi.get_player_flag(pid)) == flag.cname:
                bzapi.set_player_ricochet(pid, True)
            else:
                bzapi.set_player_ricochet(pid, False)
        except Exception:
            continue

flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
