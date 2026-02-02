from urllib.parse import quote

from karma import auth, config, views, webhttp


def handle(request):
    if request.method != "GET":
        return views.error_page("405 Method Not Allowed", "method_not_allowed")
    user = auth.get_user_from_request(request)
    if not user:
        return webhttp.redirect("/login")
    user_token = auth.user_token(user)
    profile_url = f"/users/{quote(user_token, safe='')}"
    header_html = views.header_with_title(
        config.require_setting(config.get_config(), "server.community_name"),
        "/admin-docs",
        logged_in=True,
        title=config.ui_text("titles.admin_docs"),
        user_name=auth.display_username(user),
        is_admin=auth.is_admin(user),
        profile_url=profile_url,
    )
    admin_docs_html = config.ui_text("admin_docs.html")
    diagram_alt = webhttp.html_escape(config.ui_text("admin_docs.diagram_alt"))
    body = f"""<div class="admin-docs-text">
  {admin_docs_html}
</div>
<div class="diagram">
  <img src="/static/admin-flow.svg" alt="{diagram_alt}">
</div>
"""
    return views.render_page_with_header(
        config.ui_text("titles.admin_docs"),
        header_html,
        body,
        headers=views.no_store_headers(),
    )
