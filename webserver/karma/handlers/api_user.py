from karma import config, db, webhttp
from karma.server_status import is_active


def handle(request):
    if request.method != "GET":
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")

    token = request.query.get("token", [""])[0].strip()
    username = request.query.get("name", [""])[0].strip()
    code = request.query.get("code", [""])[0].strip()
    if not (token or username or code):
        return webhttp.json_error("missing_name", status="400 Bad Request")

    settings = config.get_config()
    timeout = int(config.require_setting(settings, "heartbeat.timeout_seconds"))
    overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))

    with db.connect_ctx() as conn:
        user = None
        lookup_token = code or token
        if lookup_token:
            user = db.get_user_by_code(conn, lookup_token)
        if not user:
            lookup_name = username or token
            if lookup_name:
                user = db.get_user_by_username(conn, lookup_name)
        if not user or user["deleted"]:
            return webhttp.json_error("not_found", status="404 Not Found")
        rows = db.list_user_servers(conn, user["id"])

    active_count = 0
    inactive_count = 0
    servers = []
    for row in rows:
        active = is_active(row, timeout)
        if active:
            active_count += 1
        else:
            inactive_count += 1
        overview = row["overview"] or ""
        if overview and len(overview) > overview_max:
            overview = overview[:overview_max]
        entry = {
            "id": row["id"],
            "code": row["code"],
            "name": row["name"],
            "owner": row["owner_username"],
            "owner_code": row["owner_code"],
            "host": row["host"],
            "port": str(row["port"]),
            "overview": overview,
            "active": active,
        }
        if row["max_players"] is not None:
            entry["max_players"] = row["max_players"]
        if row["num_players"] is not None:
            entry["num_players"] = row["num_players"]
        if row["screenshot_id"] is not None:
            entry["screenshot_id"] = row["screenshot_id"]
        servers.append(entry)

    servers.sort(key=lambda item: item.get("num_players", -1), reverse=True)

    return webhttp.json_response(
        {
            "ok": True,
            "user": {"id": user["id"], "code": user["code"], "username": user["username"]},
            "active_count": active_count,
            "inactive_count": inactive_count,
            "servers": servers,
        }
    )
