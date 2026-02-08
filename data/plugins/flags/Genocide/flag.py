import bzapi
from .. import Flag

flag = Flag("genocide")

flag.name = {
    "en": "Genocide",
    "fr": "Génocide",
    "de": "Völkermord",
    "es": "Genocidio",
    "pt": "Genocídio",
    "ja": "種族滅絶",
    "ru": "Геноцид",
    "zh": "种族灭绝",
}
flag.description = {
    "en": "Killing an opponent kills the whole team.",
    "fr": "Tuer un adversaire tue toute l'équipe.",
    "de": "Das Töten eines Gegners tötet die gesamte Mannschaft.",
    "es": "Matar a un oponente mata a todo el equipo.",
    "pt": "Matar um oponente mata toda a equipe.",
    "ja": "対戦相手を殺すと、チーム全体が殺されます。",
    "ru": "Убийство противника убивает весь отряд.",
    "zh": "杀死一个对手会杀死整个队伍。"
}

flag.params = {
    'affects_teammates': True, # No effect on worlds where team kills are disabled
    'drop_flag_on_kill': 5, # False or zero to disable
    'mass_kill_cooldown': 10.0,
    'drop_flag_on_team_kill': 1 # False or zero to disable
}

# EVENT HANDLING

def on_player_hit(victim_id: int, killer_id: int, weapon_id: int, flags: int) -> None:
    # if the killer has the genocide flag, then loop over the
    # players and kill all on the victim's team.
    # Parameters should be gleaned from params dict above
    # and used for the logic.
    pass


flag.add_callback(bzapi.event_type.PLAYER_HIT, on_player_hit)

flag.register()
