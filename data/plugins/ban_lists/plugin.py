import bzapi
from plugins.bzpyapi import Command, commands

def on_player_connect(player_id: int) -> None:
    player_ip = bzapi.get_player_ip(player_id)
    banned_ips = bzapi.get_persistent_data("banlists","banned_ips") or []
    if player_ip in banned_ips:
        bzapi.disconnect_player(player_id, "You are banned from this server.")

bzapi.register_callback(bzapi.event_type.CONNECTION, on_player_connect)

#######################
# BANNING AND KICKING #
#######################

c = Command("ban-ip")
c.description = {
    "en": "Ban a player by IP for a duration (minutes) with a reason.",
    "fr": "Bannir un joueur par IP pour une durée (minutes) avec une raison."
}
c.append_usage("<ip-address> <minutes> <reason>")
c.handler = lambda tokens, from_id: (
    c.show_usage(from_id)
    if len(tokens) < 4 or not tokens[2].isdigit()
    else (
        bzapi.send_chat_message(0, from_id, f"Player '{tokens[1]}' not found.")
        if (target_id := bzapi.get_player_by_name(tokens[1])) == 0
        else (
            bzapi.ban_ip(bzapi.get_player_ip(target_id), " ".join(tokens[3:]), int(tokens[2])),
            bzapi.send_chat_message(0, from_id, f"Banned '{tokens[1]}' for {tokens[2]} minute(s).")
        )
    )
)

c = Command("ban-show-ips")
c.description = {
    "en": "List all banned IPs on this server.",
    "fr": "Lister toutes les IP bannies sur ce serveur."
}
c.handler = lambda tokens, from_id: (
    (lambda ips: c.show_info(from_id, "No banned IPs") if not ips else c.show_info(from_id, "Banned IPs: " + ", ".join(ips)))(bzapi.get_banned_ips())
)


c = Command("ban-check-ip")
c.description = {
    "en": "Check if an IP is banned.",
    "fr": "Vérifier si une IP est bannie."
}
c.append_usage("<ip>")
c.handler = lambda tokens, from_id: (
    c.show_usage(from_id)
    if len(tokens) != 2
    else (
        (lambda banned: c.show_info(from_id, f"IP {tokens[1]} is {'banned' if banned else 'not banned'}."))(tokens[1] in bzapi.get_banned_ips())
    )
)

