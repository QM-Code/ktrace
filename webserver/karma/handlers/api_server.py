from karma import config, db, webhttp
from karma.server_status import is_active


def handle(request):
    if request.method != "GET":
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")

    token = request.query.get("token", [""])[0].strip()
    name = request.query.get("name", [""])[0].strip()
    server_id = request.query.get("id", [""])[0].strip()
    code = request.query.get("code", [""])[0].strip()
    if not (token or name or server_id or code):
        return webhttp.json_error("missing_name", status="400 Bad Request")
    if server_id and not server_id.isdigit():
        return webhttp.json_error("invalid_id", status="400 Bad Request")

    with db.connect_ctx() as conn:
        server = None
        lookup_token = code or token
        if lookup_token:
            server = db.get_server_by_code(conn, lookup_token)
        if not server and lookup_token and lookup_token.isdigit():
            server = db.get_server(conn, int(lookup_token))
        if not server and server_id:
            server = db.get_server(conn, int(server_id))
        if not server:
            lookup_name = name or token
            if lookup_name:
                server = db.get_server_by_name(conn, lookup_name)
        if not server:
            return webhttp.json_error("not_found", status="404 Not Found")

    settings = config.get_config()
    timeout = int(config.require_setting(settings, "heartbeat.timeout_seconds"))
    active = is_active(server, timeout)

    payload = {
        "ok": True,
        "server": {
            "id": server["id"],
            "code": server["code"],
            "name": server["name"],
            "overview": server["overview"],
            "description": server["description"],
            "host": server["host"],
            "port": server["port"],
            "owner": server["owner_username"],
            "owner_code": server["owner_code"],
            "max_players": server["max_players"],
            "num_players": server["num_players"],
            "last_heartbeat": server["last_heartbeat"],
            "screenshot_id": server["screenshot_id"],
            "active": active,
        },
    }
    return webhttp.json_response(payload)
