import hmac
import time
import urllib.parse

from karma import auth, config, db, ratelimit, webhttp


def _handle_auth(request):
    settings = config.get_config()
    debug_auth = bool(config.require_setting(settings, "debug.auth"))
    if request.method not in ("GET", "POST"):
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")
    if request.method == "GET" and not debug_auth:
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")
    if not ratelimit.check(settings, request, "api_auth"):
        return webhttp.json_error(
            "rate_limited",
            "Too many requests. Please wait and try again.",
            status="429 Too Many Requests",
        )

    if request.method == "GET":
        source = request.query
    else:
        source = request.form()

    username = source.get("username", [""])[0].strip()
    email = source.get("email", [""])[0].strip().lower()
    password = source.get("password", [""])[0]
    passhash = source.get("passhash", [""])[0].strip()
    world = source.get("world", [""])[0].strip()
    if not debug_auth:
        password = ""
    if not (password or passhash) or (not email and not username):
        return webhttp.json_error(
            "missing_credentials",
            "username/email and password are required",
            status="400 Bad Request",
        )
    with db.connect_ctx() as conn:
        if username:
            user = db.get_user_by_username(conn, username)
        else:
            user = db.get_user_by_email(conn, email)
        if not user:
            return webhttp.json_error(
                "invalid_credentials",
                "Invalid credentials.",
                status="401 Unauthorized",
            )
        if user["is_locked"] or user["deleted"]:
            return webhttp.json_error(
                "invalid_credentials",
                "Invalid credentials.",
                status="401 Unauthorized",
            )
        if passhash:
            if not hmac.compare_digest(passhash, user["password_hash"]):
                return webhttp.json_error(
                    "invalid_credentials",
                    "Invalid credentials.",
                    status="401 Unauthorized",
                )
        else:
            if not auth.verify_password(password, user["password_salt"], user["password_hash"]):
                return webhttp.json_error(
                    "invalid_credentials",
                    "Invalid credentials.",
                    status="401 Unauthorized",
                )
        community_admin = bool(auth.is_admin(user))
        resolved_username = user["username"]
        local_admin = False
        if world:
            server = db.get_server_by_name(conn, world)
            if server and (("deleted" not in server.keys()) or not server["deleted"]):
                owner_name = server["owner_username"]
                if owner_name and owner_name.lower() == resolved_username.lower():
                    local_admin = True
                else:
                    owner_id = server["owner_user_id"]
                    direct_admins = db.list_user_admins(conn, owner_id)
                    admin_names = {admin["username"].lower() for admin in direct_admins}
                    for admin in direct_admins:
                        if not admin["trust_admins"]:
                            continue
                        for trusted in db.list_user_admins(conn, admin["admin_user_id"]):
                            admin_names.add(trusted["username"].lower())
                    local_admin = resolved_username.lower() in admin_names

        return webhttp.json_response(
            {"ok": True, "community_admin": community_admin, "local_admin": local_admin}
        )


def _handle_user_registered(request):
    if request.method not in ("GET", "POST"):
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")
    if request.method == "GET":
        username = request.query.get("username", [""])[0].strip()
    else:
        form = request.form()
        username = form.get("username", [""])[0].strip()
    if not username:
        return webhttp.json_error(
            "missing_username",
            "username is required",
            status="400 Bad Request",
        )

    settings = config.get_config()
    if not ratelimit.check(settings, request, "api_user_registered"):
        return webhttp.json_error(
            "rate_limited",
            "Too many requests. Please wait and try again.",
            status="429 Too Many Requests",
        )
    community_name = config.require_setting(settings, "server.community_name")

    with db.connect_ctx() as conn:
        user = db.get_user_by_username(conn, username)
        if not user:
            return webhttp.json_response(
                {
                    "ok": True,
                    "registered": False,
                    "community_name": community_name,
                }
            )
        return webhttp.json_response(
            {
                "ok": True,
                "registered": True,
                "community_name": community_name,
                "salt": user["password_salt"],
                "locked": bool(user["is_locked"]),
                "deleted": bool(user["deleted"]),
            }
        )


def _handle_heartbeat(request):
    if request.method not in ("GET", "POST"):
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")
    settings = config.get_config()
    debug_heartbeat = bool(config.require_setting(settings, "debug.heartbeat"))
    if request.method == "GET" and not debug_heartbeat:
        return webhttp.json_error(
            "heartbeat_debug_disabled",
            "Heartbeat debugging must be enabled in config.json to allow GET requests.",
            status="403 Forbidden",
        )
    num_players = None
    max_players = None
    new_port = None
    if request.method == "GET":
        source = request.query
    else:
        source = request.form()

    server_text = source.get("server", [""])[0].strip()
    players_text = source.get("players", [""])[0].strip()
    max_text = source.get("max", [""])[0].strip()
    newport_text = source.get("newport", [""])[0].strip()

    if not server_text:
        return webhttp.json_error(
            "missing_server",
            "server is required",
            status="400 Bad Request",
        )

    host = ""
    port = None
    if "://" in server_text:
        parsed = urllib.parse.urlparse(server_text)
        host = parsed.hostname or ""
        port = parsed.port
    else:
        if ":" in server_text:
            host, port_text = server_text.rsplit(":", 1)
        else:
            host = ""
            port_text = server_text
        try:
            port = int(port_text)
        except ValueError:
            port = None

    remote_addr = request.environ.get("REMOTE_ADDR", "")
    if not debug_heartbeat:
        host = remote_addr

    if port is None or port < 1 or port > 65535:
        return webhttp.json_error(
            "invalid_port",
            "server must include a port in the range 1-65535",
            status="400 Bad Request",
        )
    if not host and debug_heartbeat:
        return webhttp.json_error(
            "missing_host",
            "server must include a host in debug_heartbeat mode",
            status="400 Bad Request",
        )
    if not debug_heartbeat and host != remote_addr:
        return webhttp.json_error("host_mismatch", status="403 Forbidden")
    if players_text:
        try:
            num_players = int(players_text)
            if num_players < 0:
                raise ValueError
        except ValueError:
            num_players = 0
            return webhttp.json_error(
                "invalid_players",
                "players must be a non-negative integer",
                status="400 Bad Request",
            )
    if max_text:
        try:
            max_players = int(max_text)
            if max_players < 0:
                raise ValueError
        except ValueError:
            return webhttp.json_error(
                "invalid_max",
                "max must be a non-negative integer",
                status="400 Bad Request",
            )
    if max_players is not None and num_players is not None and num_players > max_players:
        return webhttp.json_error(
            "invalid_players",
            "players must be less than or equal to max",
            status="400 Bad Request",
        )

    if newport_text:
        try:
            new_port = int(newport_text)
            if new_port < 1 or new_port > 65535:
                raise ValueError
        except ValueError:
            return webhttp.json_error(
                "invalid_newport",
                "newport must be an integer in the range 1-65535",
                status="400 Bad Request",
            )
    with db.connect_ctx() as conn:
        server = db.get_server_by_host_port(conn, host, port)
        if not server:
            ports = db.list_ports_by_host(conn, host)
            if ports:
                return webhttp.json_error(
                    "port_mismatch",
                    (
                        f"Heartbeat received for {host}:{port}, but there is no game registered on port "
                        f"{port} for host {host}."
                    ),
                    status="404 Not Found",
                )
            host_label = "specified host" if debug_heartbeat else "source host"
            return webhttp.json_error(
                "host_not_found",
                f"The {host_label} ({host}) does not exist in the database of registered servers.",
                status="404 Not Found",
            )
        if new_port is not None and new_port != port:
            existing = db.get_server_by_host_port(conn, host, new_port)
            if existing:
                return webhttp.json_error(
                    "port_in_use",
                    f"newport ({new_port}) is already in use for host {host}",
                    status="409 Conflict",
                )
            db.update_server_port(conn, server["id"], new_port)
        db.update_heartbeat(conn, server["id"], int(time.time()), num_players=num_players, max_players=max_players)
        return webhttp.json_response({"ok": True})


def _handle_admins(request):
    if request.method not in ("GET", "POST"):
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")
    if request.method == "GET":
        host = request.query.get("host", [""])[0].strip()
        port_text = request.query.get("port", [""])[0].strip()
    else:
        form = request.form()
        host = form.get("host", [""])[0].strip()
        port_text = form.get("port", [""])[0].strip()
    if not host:
        return webhttp.json_error(
            "missing_host",
            "host is required",
            status="400 Bad Request",
        )
    if not port_text:
        return webhttp.json_error(
            "missing_port",
            "port is required",
            status="400 Bad Request",
        )
    try:
        port = int(port_text)
        if port < 1 or port > 65535:
            raise ValueError
    except ValueError:
        return webhttp.json_error(
            "invalid_port",
            "port must be an integer in the range 1-65535",
            status="400 Bad Request",
        )
    with db.connect_ctx() as conn:
        servers = conn.execute(
            "SELECT owner_user_id FROM servers WHERE host = ? ORDER BY id",
            (host,),
        ).fetchall()
        if not servers:
            return webhttp.json_error(
                "host_not_found",
                f"host not found: {host}",
                status="404 Not Found",
            )
        server = db.get_server_by_host_port(conn, host, port)
        if not server:
            return webhttp.json_error(
                "port_not_found",
                f"port not found for host {host}: {port}",
                status="404 Not Found",
            )
        owner_id = server["owner_user_id"]
        owner_user = db.get_user_by_id(conn, owner_id)
        if not owner_user or owner_user["deleted"]:
            return webhttp.json_error(
                "owner_not_found",
                f"owner not found for host {host}:{port}",
                status="404 Not Found",
            )
        direct_admins = db.list_user_admins(conn, owner_id)
        admin_names = {admin["username"] for admin in direct_admins}
        for admin in direct_admins:
            if not admin["trust_admins"]:
                continue
            for trusted in db.list_user_admins(conn, admin["admin_user_id"]):
                admin_names.add(trusted["username"])
        return webhttp.json_response(
            {"ok": True, "host": host, "port": port, "owner": owner_user["username"], "admins": sorted(admin_names)}
        )


def handle(request):
    path = request.path.rstrip("/")
    if path == "/api/auth":
        return _handle_auth(request)
    if path == "/api/user_registered":
        return _handle_user_registered(request)
    if path == "/api/heartbeat":
        return _handle_heartbeat(request)
    if path == "/api/admins":
        return _handle_admins(request)
    if path == "/api/health":
        if request.method != "GET":
            return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")
        return webhttp.json_response({"ok": True})
    if path == "/api/info":
        if request.method != "GET":
            return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")
        settings = config.get_config()
        return webhttp.json_response(
            {
                "ok": True,
                "community_name": config.require_setting(settings, "server.community_name"),
                "community_description": (settings.get("server") or {}).get("community_description", ""),
            }
        )
    return webhttp.json_error("not_found", status="404 Not Found")
