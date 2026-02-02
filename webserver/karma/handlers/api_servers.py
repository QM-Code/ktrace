from karma import config, db, webhttp
from karma.server_status import is_active


def handle(request, status="all"):
    if request.method != "GET":
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")

    settings = config.get_config()
    community_name = config.require_setting(settings, "server.community_name")
    overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))

    owner = request.query.get("owner", [""])[0].strip()
    status = (status or "all").lower()
    if status not in ("all", "active", "inactive"):
        return webhttp.json_error("invalid_status", status="400 Bad Request")

    with db.connect_ctx() as conn:
        if owner:
            user = db.get_user_by_username(conn, owner)
            if not user or user["deleted"]:
                return webhttp.json_error(
                    "user_not_found",
                    f"No user named {owner}.",
                    status="404 Not Found",
                )
            rows = db.list_user_servers(conn, user["id"])
        else:
            rows = db.list_servers(conn)

    servers = []
    timeout = int(config.require_setting(settings, "heartbeat.timeout_seconds"))
    active_count = 0
    inactive_count = 0
    for row in rows:
        active = is_active(row, timeout)
        if active:
            active_count += 1
        else:
            inactive_count += 1
        if status == "active" and not active:
            continue
        if status == "inactive" and active:
            continue
        entry = {
            "id": row["id"],
            "code": row["code"],
            "name": row["name"],
            "owner": row["owner_username"],
            "owner_code": row["owner_code"],
            "host": row["host"],
            "port": str(row["port"]),
            "active": active,
        }
        overview = row["overview"] or ""
        if overview and len(overview) > overview_max:
            overview = overview[:overview_max]
        if overview:
            entry["overview"] = overview
        if row["max_players"] is not None:
            entry["max_players"] = row["max_players"]
        if row["num_players"] is not None:
            entry["num_players"] = row["num_players"]
        if row["screenshot_id"] is not None:
            entry["screenshot_id"] = row["screenshot_id"]
        servers.append(entry)

    if not owner:
        servers.sort(key=lambda item: item.get("num_players", -1), reverse=True)

    payload = {
        "community_name": community_name,
        "active_count": active_count,
        "inactive_count": inactive_count,
        "servers": servers,
    }
    return webhttp.json_response(payload)
