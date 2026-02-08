import bzapi
from .. import Flag

flag = Flag("avenger")

flag.name = {
    "en": "Avenger",
    "fr": "Vengeur",
    "de": "Rächer",
    "es": "Vengador",
    "pt": "Vingador",
    "ja": "復讐者",
    "ru": "Мститель",
    "zh": "复仇者",
}

flag.description = {
    "en": "If killed by a flagged enemy, kills all enemies on that team.",
    "fr": "Si tué par un ennemi portant un drapeau, tue tous les ennemis de cette équipe.",
    "de": "Wenn du von einem beflaggten Gegner getötet wirst, tötet es alle Gegner in diesem Team.",
    "es": "Si te mata un enemigo con bandera, mata a todos los enemigos de ese equipo.",
    "pt": "Se for morto por um inimigo com bandeira, mata todos os inimigos dessa equipe.",
    "ja": "旗持ちの敵に倒されると、そのチームの敵を全員倒します。",
    "ru": "Если вас убивает враг с флагом, убивает всех врагов в этой команде.",
    "zh": "如果被持旗敌人击杀，会杀死该队的所有敌人。",
}

flag.params = {}

def on_player_hit(victim_id: int, killer_id: int, weapon_id: int, flags: int) -> bool:
    victim_flag = bzapi.get_player_flag(victim_id)
    if not victim_flag or bzapi.get_flag_cname(victim_flag) != flag.cname:
        return False

    killer_flag = bzapi.get_player_flag(killer_id)
    if not killer_flag:
        return False  # Enemy not carrying a flag, normal death

    killer_team = bzapi.get_player_team(killer_id)
    if killer_team == -1:
        return False

    # Kill all players on the killer's team (except the victim)
    for pid in bzapi.get_all_player_ids():
        if pid != victim_id and bzapi.get_player_team(pid) == killer_team:
            bzapi.kill_player(pid)
    return False  # Victim still dies normally

def on_pickup(player_id: int, flag_id: int) -> None:
    pass

def on_drop(player_id: int, flag_id: int) -> None:
    pass

flag.add_callback(bzapi.event_type.PLAYER_HIT, on_player_hit)
flag.add_callback(bzapi.event_type.FLAG_PICKUP, on_pickup)
flag.add_callback(bzapi.event_type.FLAG_DROP, on_drop)

flag.register()
