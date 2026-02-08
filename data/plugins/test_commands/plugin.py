import bzapi
from plugins.bzpyapi import Command, commands

c = Command("clientquery")
c.description = {
    "en": "Retrieve client version info from all users, or just the specified user if given.",
    "fr": "Récupérer les infos de version client de tous les joueurs ou d'un joueur donné."
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /clientquery [player_name]")
    if len(tokens) > 2
    else (
        (lambda target_ids: (
            bzapi.send_chat_message(0, from_id, "No players found.")
            if not target_ids
            else bzapi.send_chat_message(
                0,
                from_id,
                "; ".join(f"{bzapi.get_player_name(pid)}: {bzapi.get_client_info(pid)}" for pid in target_ids)
            )
        ))(
            [pid] if len(tokens) == 2 and (pid := bzapi.get_player_by_name(tokens[1])) != 0
            else ([] if len(tokens) == 2 else bzapi.get_all_player_ids())
        )
        if len(tokens) == 1 or (len(tokens) == 2 and (pid := bzapi.get_player_by_name(tokens[1])) != 0)
        else bzapi.send_chat_message(0, from_id, f"Player '{tokens[1]}' not found.")
    )
)


c = Command("countdown")
c.description = {
    "en": "Start the shutdown countdown (seconds optional).",
    "fr": "Lancer le compte à rebours d'arrêt (secondes en option)."
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /countdown [seconds]")
    if len(tokens) > 2 or (len(tokens) == 2 and not tokens[1].isdigit())
    else (
        bzapi.initiate_game_shutdown(int(tokens[1]) if len(tokens) == 2 else 10),
        bzapi.send_chat_message(0, from_id, f"Game shutdown countdown started ({int(tokens[1]) if len(tokens) == 2 else 10}s).")
    )
)

c = Command("date")
c.admin_only = False
c.description = {
    "en": "Respond with the current server date/time (same as /time).",
    "fr": "Répond avec la date/heure actuelle du serveur (comme /time)."
}
c.handler = lambda tokens, from_id: bzapi.send_chat_message(0, from_id, bzapi.get_server_datetime())


c = Command("flag")
c.description = {
    "en": "Give or take a flag namespace for a player (admin/testing).",
    "fr": "Donner ou retirer un drapeau (namespace) à un joueur (admin/tests)."
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /flag <give|take> <player_name> <flag_namespace>")
    if len(tokens) != 4 or tokens[1].lower() not in ("give", "take")
    else (
        bzapi.send_chat_message(0, from_id, f"Player '{tokens[2]}' not found.")
        if (target_id := bzapi.get_player_by_name(tokens[2])) == 0
        else (
            (bzapi.give_player_flag(target_id, tokens[3]), bzapi.send_chat_message(0, from_id, f"Gave flag '{tokens[3]}' to '{tokens[2]}'."))
            if tokens[1].lower() == "give"
            else (bzapi.drop_player_flag(target_id), bzapi.send_chat_message(0, from_id, f"Removed flag from '{tokens[2]}'."))
        )
    )
)







