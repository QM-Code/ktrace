import time
import urllib.parse

from karma import auth, config, db, markdown_utils, views, webhttp
from karma.handlers import users as users_handler
from karma.server_status import is_active


def _format_heartbeat(timestamp):
    if not timestamp:
        return config.ui_text("messages.server_profile.heartbeat_never")
    try:
        return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(int(timestamp)))
    except Exception:
        return config.ui_text("messages.server_profile.heartbeat_unknown")


def _label(key):
    return config.ui_text(f"labels.{key}", "config.json ui_text.labels")


def _action(key):
    return config.ui_text(f"actions.{key}", "config.json ui_text.actions")


def _title(key):
    return config.ui_text(f"titles.{key}", "config.json ui_text.titles")


def handle(request):
    if request.method != "GET":
        return views.error_page("405 Method Not Allowed", "method_not_allowed")

    token = request.query.get("token", [""])[0].strip()
    name = request.query.get("name", [""])[0].strip()
    code = request.query.get("code", [""])[0].strip()
    if not (token or name or code):
        return views.error_page("400 Bad Request", "missing_server")

    with db.connect_ctx() as conn:
        server = None
        lookup_token = code or token
        if lookup_token:
            server = db.get_server_by_code(conn, lookup_token)
        if not server:
            lookup_name = name or token
            if lookup_name:
                server = db.get_server_by_name(conn, lookup_name)
        if not server:
            return views.error_page("404 Not Found", "server_not_found")

        settings = config.get_config()
        timeout = int(config.require_setting(settings, "heartbeat.timeout_seconds"))
        active = is_active(server, timeout)
        user = auth.get_user_from_request(request)
        is_admin = auth.is_admin(user)
        is_owner = bool(user and server["owner_user_id"] == user["id"])
        can_manage = False
        if user and not is_owner and is_admin:
            owner_user = db.get_user_by_id(conn, server["owner_user_id"])
            levels, root_id = users_handler._admin_levels(conn, settings)
            if owner_user:
                can_manage = users_handler._can_manage_user(user, owner_user, levels, root_id)

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
            "active": active,
        }
        if is_owner or can_manage:
            server_id = server["id"]
            server_token = server["code"] or server["name"]
            csrf_html = views.csrf_input(auth.csrf_token(request))
            edit_label = webhttp.html_escape(_action("edit"))
            delete_label = webhttp.html_escape(_action("delete"))
            confirm_delete = webhttp.html_escape(config.ui_text("confirmations.delete_server"))
            entry["actions_html"] = f"""<a class="button-link secondary small" href="/server/{urllib.parse.quote(str(server_token), safe='')}/edit">{edit_label}</a>
<form method="post" action="/server/delete" data-confirm="{confirm_delete}">
  {csrf_html}
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">{delete_label}</button>
</form>"""

        profile_url = None
        if user:
            user_token = auth.user_token(user)
            profile_url = f"/users/{urllib.parse.quote(user_token, safe='')}"
        header_html = views.header(
            config.require_setting(settings, "server.community_name"),
            request.path,
            logged_in=user is not None,
            user_name=auth.display_username(user),
            is_admin=is_admin,
            profile_url=profile_url,
        )
        header_title_html = f'<span class="server-owner">{webhttp.html_escape(server["name"])}:</span> {webhttp.html_escape(_title("server_profile"))}'
        cards_html = views.render_server_cards(
            [entry],
            header_title_html=header_title_html,
        )

        description_html = markdown_utils.render_markdown(server["description"])
        if not description_html:
            empty_desc = config.require_setting(settings, "ui_text.empty_states.description")
            description_html = f'<p class="muted">{webhttp.html_escape(empty_desc)}</p>'
        heartbeat_label = webhttp.html_escape(_label("last_heartbeat"))
        info_html = f"""<div class="info-panel">
  <div><strong>{heartbeat_label}:</strong> {_format_heartbeat(server["last_heartbeat"])}</div>
</div>"""

        description_section = f"""<div class="info-panel">
  <div><strong>{webhttp.html_escape(_label("description"))}</strong></div>
  {description_html}
</div>"""

        body = f"""{cards_html}
{description_section}
{info_html}
"""
        return views.render_page_with_header(_title("server_profile"), header_html, body)
