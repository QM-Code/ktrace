import bzapi

PREFIX = "/"
LANG = "pt"
commands = {}

class Command:
    def __init__(self, name: str, activate: bool = True):
        self.name = name
        self.handler = None
        self.admin_only = True
        self.description = {}
        self.aliases = []
        self.usage = ""
        if activate:
            commands[self.name] = self

    def append_usage(self, usage: str) -> None:
        self.usage += f" {usage}"

    def show_usage(self, to_player_id: int) -> None:
        USAGE = {
            "en": "Usage",
            "fr": "Utilisation",
            "de": "Verwendung",
            "es": "Uso",
            "pt": "Uso",
            "ja": "使い方",
            "ru": "Использование",
            "zh": "用法"
        }
        #usage = USAGE.get(bzapi.get_player_language(to_player_id), "Usage")
        usage = USAGE.get(LANG, "Usage")
        usage += ": " + PREFIX + self.name
        if self.usage:
            usage += self.usage
        bzapi.send_chat_message(0, to_player_id, usage)

    def show_info(self, to_player_id: int, info: str) -> None:
        bzapi.send_chat_message(0, to_player_id, info)

    def get_description(self, lang=LANG):
        if lang in self.description:
            return self.description[lang]
        return next(iter(self.description.values()), "")



def handler(from_id: int, to_id: int, text: str):
    if not text or not text.startswith(PREFIX):
        return False
    tokens = text[len(PREFIX):].split()
    if not tokens:
        return False
    cmd = tokens[0].lower()
    command = commands.get(cmd)
    if not command or not command.handler:
        return False
    #if command.admin_only and not bzapi.is_player_admin(from_id):
    #    bzapi.send_chat_message(0, from_id, "Admin only command.")
    #    return True
    command.handler(tokens, from_id)
    return True

bzapi.register_callback(bzapi.EventType.CHAT, handler)


c = Command("?")
c.admin_only = False
c.append_usage("[command]")
c.description = {
    "en": "List all available commands, or detailed info for a specific command.",
    "fr": "Lister toutes les commandes disponibles, ou des informations détaillées pour une commande spécifique.",
    "de": "Alle verfügbaren Befehle auflisten oder detaillierte Informationen zu einem bestimmten Befehl anzeigen.",
    "es": "Listar todos los comandos disponibles o información detallada para un comando específico.",
    "pt": "Listar todos os comandos disponíveis ou informações detalhadas para um comando específico.",
    "ja": "利用可能なすべてのコマンド、または特定のコマンドの詳細情報を一覧表示します。",
    "ru": "Список всех доступных команд или подробная информация о конкретной команде.",
    "zh": "列出所有可用命令，或特定命令的详细信息。"
}
def _help_handler(tokens, from_id):
    if len(tokens) == 1:
        entries = []
        for name, command in sorted(commands.items()):
            description = command.get_description(LANG)
            if description:
                entries.append(f"{name} - {description}")
            else:
                entries.append(name)
        msg = "Commands:\n" + "\n".join(entries)
        bzapi.send_chat_message(0, from_id, msg)
        return

    target_cmd = tokens[1].lower()
    command = commands.get(target_cmd)
    if not command:
        bzapi.send_chat_message(0, from_id, f"Unknown command '{target_cmd}'.")
        return

    command.show_usage(from_id)
    description = command.get_description(LANG)
    if description:
        bzapi.send_chat_message(0, from_id, description)
c.handler = _help_handler



flags = {}

class Flag:
    def __init__(self, cname: str, activate: bool = True):
        self.cname = cname
        self.name = {}
        self.description = {}
        self.params = {}
        self.callbacks = {}
        if activate:
            flags[self.cname] = self

    def set_parameter(self, key: str, value: object) -> None:
        bzapi.set_data("flags/"+self.cname, key, self.params.get(key))
        self.params[key] = value
    
    def get_parameter(self, key: str) -> object:
        return bzapi.get_data("flags/"+self.cname, key) or self.params.get(key)
    
    def add_callback(self, EventType: bzapi.EventType, callback) -> None:
        self.callbacks[EventType] = callback
    
    def register(self) -> None:
        bzapi.register_flag(self.cname, self.name, self.description, self.params)
        for event, callback in self.callbacks.items():
            bzapi.register_callback(event, callback)
