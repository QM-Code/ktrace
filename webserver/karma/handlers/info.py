from urllib.parse import quote

from karma import auth, config, markdown_utils, views, webhttp


def _label(key):
    return config.ui_text(f"labels.{key}", "config.json ui_text.labels")


def _action(key):
    return config.ui_text(f"actions.{key}", "config.json ui_text.actions")


def _title(key):
    return config.ui_text(f"titles.{key}", "config.json ui_text.titles")


def _render_info_page(request, message=None, error=None, edit_mode=False):
    settings = config.get_config()
    community_name = config.require_setting(settings, "server.community_name")
    community_description = (settings.get("server") or {}).get("community_description", "")

    user = auth.get_user_from_request(request)
    is_admin = auth.is_admin(user)
    profile_url = None
    if user:
        user_token = auth.user_token(user)
        profile_url = f"/users/{quote(user_token, safe='')}"

    header_html = views.header_with_title(
        community_name,
        "/info",
        logged_in=user is not None,
        title=_title("community_info"),
        user_name=auth.display_username(user),
        is_admin=is_admin,
        profile_url=profile_url,
        info=message,
        error=error,
    )

    description_html = markdown_utils.render_markdown(community_description)
    if not description_html:
        empty_desc = config.require_setting(settings, "ui_text.empty_states.community_description")
        description_html = f'<p class="muted">{webhttp.html_escape(empty_desc)}</p>'
    info_panel = f"""<div class="info-panel">
  <strong>{webhttp.html_escape(community_name)}</strong>
  <div>{description_html}</div>
</div>"""

    edit_link_html = ""
    if is_admin and not edit_mode:
        edit_link_html = """<div class="actions section-actions">
  <a class="admin-link" href="/info?edit=1">{edit_label}</a>
</div>"""
        edit_link_html = edit_link_html.format(edit_label=webhttp.html_escape(_action("edit_info")))

    form_html = ""
    if is_admin and edit_mode:
        csrf_html = views.csrf_input(auth.csrf_token(request))
        safe_name = webhttp.html_escape(community_name)
        safe_description = webhttp.html_escape(community_description)
        markdown_hint = config.require_setting(settings, "ui_text.hints.markdown_supported")
        form_html = f"""<form method="post" action="/info">
  {csrf_html}
  <label for="community_name">{webhttp.html_escape(_label("community_name"))}</label>
  <input id="community_name" name="community_name" value="{safe_name}" required>
  <label for="community_description">{webhttp.html_escape(_label("community_description"))}</label>
  <textarea id="community_description" name="community_description" rows="6">{safe_description}</textarea>
  <p class="muted hint">{webhttp.html_escape(markdown_hint)}</p>
  <div class="actions section-actions">
    <button type="submit">{webhttp.html_escape(_action("save"))}</button>
    <a class="admin-link secondary" href="/info">{webhttp.html_escape(_action("cancel"))}</a>
  </div>
</form>"""

    body = f"""{info_panel}
{edit_link_html}
{form_html}
"""
    return views.render_page_with_header(
        _title("community_info"),
        header_html,
        body,
        headers=views.no_store_headers(),
    )


def handle(request):
    if request.method not in ("GET", "POST"):
        return views.error_page("405 Method Not Allowed", "method_not_allowed")

    user = auth.get_user_from_request(request)
    is_admin = auth.is_admin(user)

    if request.method == "POST":
        if not is_admin:
            return views.error_page("403 Forbidden", "forbidden")
        form = request.form()
        if not auth.verify_csrf(request, form):
            return views.error_page("403 Forbidden", "forbidden")
        name = form.get("community_name", [""])[0].strip()
        description = form.get("community_description", [""])[0].strip()
        if not name:
            return _render_info_page(request, error="Community name is required.", edit_mode=True)
        community_settings = dict(config.get_community_config())
        community_settings.setdefault("server", {})
        community_settings["server"]["community_name"] = name
        community_settings["server"]["community_description"] = description
        config.save_community_config(community_settings)
        return webhttp.redirect("/info?updated=1")

    updated = request.query.get("updated", [""])[0] == "1"
    edit_mode = request.query.get("edit", [""])[0] == "1"
    message = config.ui_text("messages.info.updated", "config.json ui_text.messages.info") if updated else None
    return _render_info_page(request, message=message, edit_mode=edit_mode)
