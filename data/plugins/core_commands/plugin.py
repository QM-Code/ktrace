import bzapi
from plugins.bzpyapi import Command, LANG

c = Command("kick")
c.description = {
    "en": "Kick a named player off the server.",
    "fr": "Expulser un joueur nommé du serveur.",
    "de": "Einen benannten Spieler vom Server kicken.",
    "es": "Expulsar a un jugador nombrado del servidor.",
    "pt": "Expulsar um jogador nomeado do servidor.",
    "ja": "指定したプレイヤーをサーバーからキックします。",
    "ru": "Выгнать указанного игрока с сервера.",
    "zh": "将指定的玩家踢出服务器。"
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /kick <player_name>")
    if len(tokens) != 2
    else (
        bzapi.send_chat_message(0, from_id, f"Player '{tokens[1]}' not found.")
        if (target_id := bzapi.get_player_by_name(tokens[1])) == 0
        else (
            bzapi.kick_player(target_id, "Kicked by server"),
            bzapi.send_chat_message(0, from_id, f"Kicked '{tokens[1]}'.")
        )
    )
)


c = Command("kill")
c.description = {
    "en": "Kill a player (shows as destroyed by the server).",
    "fr": "Tuer un joueur (affiché comme détruit par le serveur).",
    "de": "Einen Spieler töten (wird als vom Server zerstört angezeigt).",
    "es": "Matar a un jugador (se muestra como destruido por el servidor).",
    "pt": "Matar um jogador (mostra como destruído pelo servidor).",
    "ja": "プレイヤーをキルします（サーバーによって破壊されたように表示されます）。",
    "ru": "Убить игрока (показывается как уничтоженный сервером).",
    "zh": "杀死一个玩家（显示为被服务器摧毁）。"
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /kill <player_name>")
    if len(tokens) != 2
    else (
        bzapi.send_chat_message(0, from_id, f"Player '{tokens[1]}' not found.")
        if (target_id := bzapi.get_player_by_name(tokens[1])) == 0
        else (
            bzapi.kill_player(target_id),
            bzapi.send_chat_message(0, from_id, f"Killed '{tokens[1]}'.")
        )
    )
)

c = Command("reset")
c.description = {
    "en": "Reset a server variable to its default setting.",
    "fr": "Réinitialiser un paramètre serveur à sa valeur par défaut.",
    "de": "Setzt eine Servervariable auf ihre Standardeinstellung zurück.",
    "es": "Restablecer una variable del servidor a su configuración predeterminada.",
    "pt": "Redefinir uma variável do servidor para sua configuração padrão.",
    "ja": "サーバー変数をデフォルト設定にリセットします。",
    "ru": "Сбросить серверную переменную к ее настройке по умолчанию.",
    "zh": "将服务器变量重置为其默认设置。"
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /reset <setting>")
    if len(tokens) != 2
    else (
        bzapi.reset_parameter(tokens[1]),
        bzapi.send_chat_message(0, from_id, f"World setting '{tokens[1]}' reset to default.")
    )
)

c = Command("set")
c.description = {
    "en": "Set a server variable to a specific value.",
    "fr": "Définir une variable serveur à une valeur spécifique.",
    "de": "Setzt eine Servervariable auf einen bestimmten Wert.",
    "es": "Establecer una variable del servidor a un valor específico.",
    "pt": "Definir uma variável do servidor para um valor específico.",
    "ja": "サーバー変数を特定の値に設定します。",
    "ru": "Установить серверную переменную на определенное значение.",
    "zh": "将服务器变量设置为特定值。"
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /set <setting> <value>")
    if len(tokens) != 3
    else (
        bzapi.send_chat_message(0, from_id, "Value must be a number.")
        if not (lambda: (float(tokens[2]), True)[1])()
        else (
            bzapi.set_parameter(tokens[1], float(tokens[2])),
            bzapi.send_chat_message(0, from_id, f"World setting '{tokens[1]}' set to {float(tokens[2])}.")
        )
    )
)



c = Command("kickall")
c.description = {
    "en": "Kick all players on a team, or everyone if no team is given.",
    "fr": "Expulser tous les joueurs d'une équipe, ou tout le monde si aucune équipe n'est donnée.",
    "de": "Alle Spieler eines Teams kicken oder alle, wenn kein Team angegeben ist.",
    "es": "Expulsar a todos los jugadores de un equipo, o a todos si no se da ningún equipo.",
    "pt": "Expulsar todos os jogadores de uma equipe, ou todos se nenhuma equipe for dada.",
    "ja": "チームの全プレイヤーをキックするか、チームが指定されていない場合は全員をキックします。",
    "ru": "Выгнать всех игроков из команды или всех, если команда не указана.",
    "zh": "踢出一个队伍的所有玩家，如果没有给出队伍则踢出所有玩家。"
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /superkick [team_id]")
    if len(tokens) > 2 or (len(tokens) == 2 and not tokens[1].isdigit())
    else (
        (lambda team_filter: (
            (lambda ids: (
                bzapi.send_chat_message(0, from_id, "No players found to kick.") if not ids else (
                    [bzapi.kick_player(pid, f"Kicked by server{' (team ' + str(team_filter) + ')' if team_filter is not None else ''}") for pid in ids],
                    bzapi.send_chat_message(0, from_id, f"Kicked {len(ids)} player(s).")
                )
            ))([pid for pid in bzapi.get_all_player_ids() if team_filter is None or bzapi.get_player_team(pid) == team_filter])
        ))(None if len(tokens) == 1 else int(tokens[1]))
    )
)


c = Command("killall")
c.description = {
    "en": "Kill all players on a team, or everyone if no team is given.",
    "fr": "Tuer tous les joueurs d'une équipe, ou tout le monde si aucune équipe n'est donnée.",
    "de": "Alle Spieler eines Teams töten oder alle, wenn kein Team angegeben ist.",
    "es": "Matar a todos los jugadores de un equipo, o a todos si no se da ningún equipo.",
    "pt": "Matar todos os jogadores de uma equipe, ou todos se nenhuma equipe for dada.",
    "ja": "チームの全プレイヤーをキルするか、チームが指定されていない場合は全員をキルします。",
    "ru": "Убить всех игроков из команды или всех, если команда не указана.",
    "zh": "杀死一个队伍的所有玩家，如果没有给出队伍则杀死所有玩家。"
}
c.handler = lambda tokens, from_id: (
    bzapi.send_chat_message(0, from_id, "Usage: /superkill [team_id]")
    if len(tokens) > 2 or (len(tokens) == 2 and not tokens[1].isdigit())
    else (
        (lambda team_filter: (
            (lambda ids: (
                bzapi.send_chat_message(0, from_id, "No players found to kill.") if not ids else (
                    [bzapi.kill_player(pid) for pid in ids],
                    bzapi.send_chat_message(0, from_id, f"Killed {len(ids)} player(s).")
                )
            ))([pid for pid in bzapi.get_all_player_ids() if team_filter is None or bzapi.get_player_team(pid) == team_filter])
        ))(None if len(tokens) == 1 else int(tokens[1]))
    )
)


c = Command("msg")
c.admin_only = False
c.description = {
    "en": 'Send a private message to another player.',
    "fr": 'Envoyer un message privé à un autre joueur.',
    "de": 'Sende eine private Nachricht an einen anderen Spieler.',
    "es": 'Enviar un mensaje privado a otro jugador.',
    "pt": 'Enviar uma mensagem privada para outro jogador.',
    "ja": '他のプレイヤーにプライベートメッセージを送信します。',
    "ru": 'Отправить личное сообщение другому игроку.',
    "zh": '向另一位玩家发送私人消息。'
}
def _msg_handler(tokens, from_id):
    if len(tokens) < 3:
        bzapi.send_chat_message(0, from_id, "Usage: /msg <player_name> <message>")
        return

    target_id = bzapi.get_player_by_name(tokens[1])
    if target_id == 0:
        bzapi.send_chat_message(0, from_id, f"Player '{tokens[1]}' not found.")
        return

    if target_id == from_id:
        self_msg = {
            "en": "Talking to yourself is a sign of dementia.",
            "fr": "Se parler à soi-même est un signe de démence.",
            "de": "Mit sich selbst zu reden ist ein Zeichen von Demenz.",
            "es": "Hablar contigo mismo es un signo de demencia.",
            "pt": "Falar consigo mesmo é um sinal de demência.",
            "ja": "独り言は認知症の兆候です。",
            "ru": "Разговоры с самим собой — признак деменции.",
            "zh": "自言自语是痴呆的征兆。"
        }
        bzapi.send_chat_message(0, from_id, self_msg.get(LANG, self_msg["en"]))
        return

    bzapi.send_chat_message(from_id, target_id, " ".join(tokens[2:]))

c.handler = _msg_handler


c = Command("quit")
c.admin_only = False
c.description = {
    "en": 'Leave the server with a goodbye message.',
    "fr": 'Quitter le serveur avec un message d\'au revoir.',
    "de": 'Den Server mit einer Abschiedsnachricht verlassen.',
    "es": 'Salir del servidor con un mensaje de despedida.',
    "pt": 'Sair do servidor com uma mensagem de despedida.',
    "ja": 'さようならメッセージとともにサーバーを離れます。',
    "ru": 'Покинуть сервер с прощальным сообщением.',
    "zh": '带着告别信息离开服务器。'
}
c.handler = lambda tokens, from_id: (
    (lambda msg: (bzapi.send_chat_message(0, 0, msg), bzapi.disconnect_player(from_id, msg)))(
        f"{bzapi.get_player_name(from_id)} has left the server." + ("" if len(tokens) == 1 else " " + " ".join(tokens[1:]))
    )
)
