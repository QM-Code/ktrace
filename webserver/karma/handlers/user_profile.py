from urllib.parse import quote

from karma import auth, config, db, views, webhttp
from karma.handlers import account, users as users_handler


def _can_manage_profile(current_user, target_user, conn, settings):
    if not current_user:
        return False
    if current_user["id"] == target_user["id"]:
        return True
    if not auth.is_admin(current_user):
        return False
    levels, root_id = users_handler._admin_levels(conn, settings)
    return users_handler._can_manage_user(current_user, target_user, levels, root_id)


def _profile_url(username):
    return f"/users/{quote(username, safe='')}"


def _msg(key, **values):
    template = config.ui_text(f"messages.users.{key}", "config.json ui_text.messages.users")
    return config.format_text(template, **values)


def _label(key):
    return config.ui_text(f"labels.{key}", "config.json ui_text.labels")


def _action(key):
    return config.ui_text(f"actions.{key}", "config.json ui_text.actions")


def _section(key):
    return config.ui_text(f"sections.{key}", "config.json ui_text.sections")


def _title(key):
    return config.ui_text(f"titles.{key}", "config.json ui_text.titles")


def _render_profile(
    request,
    target_user,
    current_user,
    servers,
    admins,
    can_manage,
    message=None,
    admin_notice="",
):
    settings = config.get_config()
    timeout = int(config.require_setting(settings, "heartbeat.timeout_seconds"))
    overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))
    safe_username = webhttp.html_escape(target_user["username"])
    header_title_html = f'<span class="server-owner">{safe_username}:</span> {webhttp.html_escape(_section("servers"))}'

    def _entry_builder(server, active):
        entry = {
            "id": server["id"],
            "code": server["code"],
            "host": server["host"],
            "port": str(server["port"]),
            "name": server["name"],
            "overview": server["overview"] or "",
            "max_players": server["max_players"],
            "num_players": server["num_players"],
            "owner": server["owner_username"],
            "owner_code": server["owner_code"],
            "screenshot_id": server["screenshot_id"],
        }
        if can_manage:
            server_id = entry.get("id")
            server_token = entry.get("code") or entry.get("name") or f"{entry['host']}:{entry['port']}"
            csrf_html = views.csrf_input(auth.csrf_token(request))
            confirm_delete = webhttp.html_escape(config.ui_text("confirmations.delete_server"))
            entry["actions_html"] = f"""<a class="button-link secondary small" href="/server/{quote(str(server_token), safe='')}/edit">{webhttp.html_escape(_action("edit"))}</a>
<form method="post" action="/server/delete" data-confirm="{confirm_delete}">
  {csrf_html}
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">{webhttp.html_escape(_action("delete"))}</button>
</form>"""
        overview = entry.get("overview")
        if overview and len(overview) > overview_max:
            entry["overview"] = overview[:overview_max]
        return entry

    user_token = auth.user_token(target_user)
    encoded_user = quote(user_token)
    server_page = config.require_setting(settings, "pages.servers")
    refresh_interval = int(config.require_setting(server_page, "auto_refresh", "config.json pages.servers") or 0)
    refresh_animate = bool(config.require_setting(server_page, "auto_refresh_animate", "config.json pages.servers"))
    refresh_url = None
    if refresh_interval > 0:
        refresh_url = f"/api/servers?owner={quote(target_user['username'], safe='')}"
    status = "all"
    header_actions_html = ""
    if can_manage:
        header_actions_html = """<div class="actions">
  <a class="admin-link" href="/servers/add?owner={username}">{add_server}</a>
  <a class="admin-link secondary" href="/users/{username}/edit">{profile}</a>
</div>"""
        header_actions_html = header_actions_html.format(
            username=encoded_user,
            add_server=webhttp.html_escape(_action("add_server")),
            profile=webhttp.html_escape(_action("personal_settings")),
        )
    cards_html = views.render_server_section(
        servers,
        timeout,
        status,
        _entry_builder,
        header_title_html=header_title_html,
        header_actions_html=header_actions_html or None,
        csrf_token=auth.csrf_token(request),
        refresh_url=refresh_url,
        refresh_interval=refresh_interval,
        allow_actions=can_manage,
        refresh_animate=refresh_animate,
        sort_entries=False,
    )

    admins_header_html = f'<span class="server-owner">{safe_username}:</span> {webhttp.html_escape(_section("admins"))}'
    admins_section = views.render_admins_section(
        admins,
        show_controls=can_manage,
        show_add_form=can_manage,
        form_prefix=f"/users/{encoded_user}",
        notice_html=admin_notice,
        header_title_html=admins_header_html,
        csrf_token=auth.csrf_token(request),
    )

    profile_url = None
    if current_user:
        current_token = auth.user_token(current_user)
        profile_url = _profile_url(current_token)
    header_html = views.header(
        config.require_setting(settings, "server.community_name"),
        f"/users/{encoded_user}",
        logged_in=current_user is not None,
        user_name=auth.display_username(current_user),
        is_admin=auth.is_admin(current_user),
        profile_url=profile_url,
        error=message,
    )
    body = f"""{cards_html}
<hr class="section-divider">
{admins_section}
"""
    return views.render_page_with_header(
        _title("user_profile"),
        header_html,
        body,
        headers=views.no_store_headers(),
    )


def handle(request):
    if request.method not in ("GET", "POST"):
        return views.error_page("405 Method Not Allowed", "method_not_allowed")

    token = request.query.get("token", [""])[0].strip()
    username = request.query.get("name", [""])[0].strip()
    code = request.query.get("code", [""])[0].strip()
    if not (token or username or code):
        return views.error_page("400 Bad Request", "missing_user")

    settings = config.get_config()
    with db.connect_ctx() as conn:
        target_user = None
        lookup_token = code or token
        if lookup_token:
            target_user = db.get_user_by_code(conn, lookup_token)
        if not target_user:
            lookup_name = username or token
            if lookup_name:
                target_user = db.get_user_by_username(conn, lookup_name)
        if not target_user:
            return views.error_page("404 Not Found", "user_not_found")

        current_user = auth.get_user_from_request(request)
        is_admin = auth.is_admin(current_user)
        if target_user["deleted"] and not is_admin:
            return views.error_page("404 Not Found", "user_not_found")
        if current_user:
            account._sync_root_admin_privileges(conn, settings)
        can_manage = _can_manage_profile(current_user, target_user, conn, settings)
        path = request.path.rstrip("/")
        if request.method == "POST":
            if not can_manage:
                return views.error_page("403 Forbidden", "forbidden")
            form = request.form()
            if not auth.verify_csrf(request, form):
                return views.error_page("403 Forbidden", "forbidden")
            remainder = path[len("/users/") :]
            parts = remainder.split("/", 2)
            action = parts[2] if len(parts) == 3 and parts[1] == "admins" else ""
            message = None
            if not action:
                return views.error_page("404 Not Found", "not_found")
            if action == "add":
                username_input = form.get("username", [""])[0].strip()
                if not username_input:
                    message = _msg("username_required")
                elif username_input.lower() == target_user["username"].lower():
                    message = _msg("username_self_add")
                else:
                    admin_user = db.get_user_by_username(conn, username_input)
                    if not admin_user:
                        message = _msg("user_not_found")
                    else:
                        db.add_user_admin(conn, target_user["id"], admin_user["id"])
                        account._recompute_root_admins(conn, target_user, settings)
                        target_token = auth.user_token(target_user)
                        return webhttp.redirect(_profile_url(target_token))
            elif action == "trust":
                username_input = form.get("username", [""])[0].strip()
                trust = form.get("trust_admins", [""])[0] == "1"
                admin_user = db.get_user_by_username(conn, username_input)
                if not admin_user:
                    message = _msg("user_not_found")
                else:
                    db.set_user_admin_trust(conn, target_user["id"], admin_user["id"], trust)
                    account._recompute_root_admins(conn, target_user, settings)
                    target_token = auth.user_token(target_user)
                    return webhttp.redirect(_profile_url(target_token))
            elif action == "remove":
                username_input = form.get("username", [""])[0].strip()
                if username_input:
                    admin_user = db.get_user_by_username(conn, username_input)
                    if admin_user:
                        db.remove_user_admin(conn, target_user["id"], admin_user["id"])
                        account._recompute_root_admins(conn, target_user, settings)
                        target_token = auth.user_token(target_user)
                        return webhttp.redirect(_profile_url(target_token))
            servers = []
            if not target_user["deleted"]:
                servers = db.list_user_servers(conn, target_user["id"])
            admins = db.list_user_admins(conn, target_user["id"])
            notice_html = ""
            if current_user and current_user["id"] == target_user["id"]:
                notice_html = account._trusted_primary_notice(conn, current_user, settings)
            return _render_profile(
                request,
                target_user,
                current_user,
                servers,
                admins,
                can_manage,
                message=message,
                admin_notice=notice_html,
            )

        servers = []
        if not target_user["deleted"]:
            servers = db.list_user_servers(conn, target_user["id"])
        admins = db.list_user_admins(conn, target_user["id"])
        notice_html = ""
        if current_user and current_user["id"] == target_user["id"]:
            notice_html = account._trusted_primary_notice(conn, current_user, settings)
        return _render_profile(
            request,
            target_user,
            current_user,
            servers,
            admins,
            can_manage,
            admin_notice=notice_html,
        )
