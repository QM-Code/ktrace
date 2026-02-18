import io
import json
import os
import tempfile
import unittest
import urllib.parse


def _load_modules():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if root_dir not in os.sys.path:
        os.sys.path.insert(0, root_dir)
    from karma import auth, config, db, webhttp
    from karma.handlers import server_edit, user_profile, users

    return auth, config, db, webhttp, users, user_profile, server_edit


def _make_request(webhttp, method, path, query_string="", form_data=None, cookie="", extra_environ=None):
    body = b""
    content_type = ""
    if form_data is not None:
        body = urllib.parse.urlencode(form_data).encode("utf-8")
        content_type = "application/x-www-form-urlencoded"
    environ = {
        "REQUEST_METHOD": method,
        "PATH_INFO": path,
        "QUERY_STRING": query_string,
        "wsgi.input": io.BytesIO(body or b""),
        "CONTENT_LENGTH": str(len(body)),
        "CONTENT_TYPE": content_type,
        "REMOTE_ADDR": "127.0.0.1",
    }
    if cookie:
        environ["HTTP_COOKIE"] = cookie
    if extra_environ:
        environ.update(extra_environ)
    return webhttp.Request(environ)


class _MutationFlowBase(unittest.TestCase):
    def setUp(self):
        (
            self.auth,
            self.config,
            self.db,
            self.webhttp,
            self.users_handler,
            self.user_profile_handler,
            self.server_edit_handler,
        ) = _load_modules()
        self.temp_dir = tempfile.TemporaryDirectory(prefix="karma-unit-community-")
        self.addCleanup(self.temp_dir.cleanup)
        self._configure_community()
        self._seed_data()

    def _configure_community(self):
        community_config = {
            "server": {
                "community_name": "Unit Test Community",
                "admin_user": "Admin",
                "session_secret": "unit-test-secret",
                "language": "en",
            }
        }
        config_path = os.path.join(self.temp_dir.name, "config.json")
        with open(config_path, "w", encoding="utf-8") as handle:
            json.dump(community_config, handle)
            handle.write("\n")
        self.config.set_community_dir(self.temp_dir.name)
        self.db.init_db(self.db.default_db_path())

    def _seed_data(self):
        with self.db.connect_ctx() as conn:
            self._add_user(conn, "Admin", "admin@example.local", is_admin=True, is_admin_manual=True)
            self._add_user(conn, "owner_user", "owner@example.local")
            self._add_user(conn, "helper_user", "helper@example.local")
            self._add_user(conn, "extra_user", "extra@example.local")

            owner = self.db.get_user_by_username(conn, "owner_user")
            self.assertIsNotNone(owner)
            self.db.add_server(
                conn,
                {
                    "name": "owner-server",
                    "overview": "Owner overview",
                    "description": "Owner description",
                    "host": "127.0.0.1",
                    "port": 12950,
                    "owner_user_id": owner["id"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 0,
                    "last_heartbeat": None,
                },
            )
            self.db.recompute_admin_flags(conn, self._user(conn, "Admin")["id"])

        with self.db.connect_ctx() as conn:
            self.users = {
                "Admin": self._user(conn, "Admin"),
                "owner_user": self._user(conn, "owner_user"),
                "helper_user": self._user(conn, "helper_user"),
                "extra_user": self._user(conn, "extra_user"),
            }
            self.server = self.db.get_server_by_name(conn, "owner-server")
            self.assertIsNotNone(self.server)
            self.user_tokens = {
                username: self.auth.user_token(user) for username, user in self.users.items()
            }
            self.session_cookies = {
                username: f"user_session={self.auth.sign_user_session(user['id'])}"
                for username, user in self.users.items()
            }

    def _add_user(self, conn, username, email, is_admin=False, is_admin_manual=False):
        digest, salt = self.auth.new_password("pw-" + username)
        self.db.add_user(
            conn,
            username,
            email,
            digest,
            salt,
            is_admin=is_admin,
            is_admin_manual=is_admin_manual,
        )

    def _user(self, conn, username):
        user = self.db.get_user_by_username(conn, username)
        self.assertIsNotNone(user)
        return user

    def _csrf_for_cookie(self, cookie):
        request = _make_request(self.webhttp, "GET", "/csrf", cookie=cookie)
        token = self.auth.csrf_token(request)
        self.assertTrue(token)
        return token

    def _location_header(self, headers):
        for key, value in headers:
            if key == "Location":
                return value
        return None


class UsersMutationFlowTest(_MutationFlowBase):
    def test_create_user_with_admin_checkbox_adds_root_admin_link(self):
        admin_cookie = self.session_cookies["Admin"]
        csrf_token = self._csrf_for_cookie(admin_cookie)
        request = _make_request(
            self.webhttp,
            "POST",
            "/users/create",
            form_data={
                "csrf_token": csrf_token,
                "username": "new_admin",
                "email": "new_admin@example.local",
                "password": "pw-new-admin",
                "is_admin": "on",
            },
            cookie=admin_cookie,
        )

        status, headers, body = self.users_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), "/users")

        with self.db.connect_ctx() as conn:
            created = self.db.get_user_by_username(conn, "new_admin")
            self.assertIsNotNone(created)
            self.assertTrue(bool(created["is_admin"]))
            root = self._user(conn, "Admin")
            admins = self.db.list_user_admins(conn, root["id"])
            admin_ids = {row["admin_user_id"] for row in admins}
            self.assertIn(created["id"], admin_ids)

    def test_lock_user_mutation_sets_locked_flag(self):
        admin_cookie = self.session_cookies["Admin"]
        csrf_token = self._csrf_for_cookie(admin_cookie)
        target_user_id = self.users["helper_user"]["id"]
        request = _make_request(
            self.webhttp,
            "POST",
            "/users/lock",
            form_data={
                "csrf_token": csrf_token,
                "id": str(target_user_id),
                "locked": "1",
            },
            cookie=admin_cookie,
        )

        status, headers, body = self.users_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), "/users")

        with self.db.connect_ctx() as conn:
            updated = self.db.get_user_by_id(conn, target_user_id)
            self.assertIsNotNone(updated)
            self.assertTrue(bool(updated["is_locked"]))

    def test_self_edit_mutation_updates_email_language_and_password(self):
        owner_cookie = self.session_cookies["owner_user"]
        owner_token = self.user_tokens["owner_user"]
        csrf_token = self._csrf_for_cookie(owner_cookie)
        with self.db.connect_ctx() as conn:
            before = self._user(conn, "owner_user")
            old_hash = before["password_hash"]

        request = _make_request(
            self.webhttp,
            "POST",
            f"/users/{urllib.parse.quote(owner_token, safe='')}/edit",
            query_string=f"token={urllib.parse.quote(owner_token, safe='')}",
            form_data={
                "csrf_token": csrf_token,
                "email": "owner-updated@example.local",
                "language": "en",
                "password": "pw-owner-updated",
            },
            cookie=owner_cookie,
        )

        status, headers, body = self.users_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), f"/users/{urllib.parse.quote(owner_token, safe='')}")

        with self.db.connect_ctx() as conn:
            updated = self._user(conn, "owner_user")
            self.assertEqual(updated["email"], "owner-updated@example.local")
            self.assertEqual(updated["language"], "en")
            self.assertNotEqual(updated["password_hash"], old_hash)

    def test_create_user_rejects_invalid_csrf_without_mutation(self):
        admin_cookie = self.session_cookies["Admin"]
        request = _make_request(
            self.webhttp,
            "POST",
            "/users/create",
            form_data={
                "csrf_token": "invalid-token",
                "username": "should_not_exist",
                "email": "should_not_exist@example.local",
                "password": "pw-should-not-exist",
                "is_admin": "on",
            },
            cookie=admin_cookie,
        )

        status, headers, body = self.users_handler.handle(request)
        self.assertEqual(status, "403 Forbidden")
        self.assertTrue(any(key == "Content-Type" for key, _ in headers))
        self.assertIn(b"Forbidden", body)

        with self.db.connect_ctx() as conn:
            self.assertIsNone(self.db.get_user_by_username(conn, "should_not_exist"))

    def test_lock_user_requires_admin_permission_without_mutation(self):
        helper_cookie = self.session_cookies["helper_user"]
        helper_csrf = self._csrf_for_cookie(helper_cookie)
        target_user_id = self.users["owner_user"]["id"]
        request = _make_request(
            self.webhttp,
            "POST",
            "/users/lock",
            form_data={
                "csrf_token": helper_csrf,
                "id": str(target_user_id),
                "locked": "1",
            },
            cookie=helper_cookie,
        )

        status, headers, body = self.users_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), "/users")

        with self.db.connect_ctx() as conn:
            target = self.db.get_user_by_id(conn, target_user_id)
            self.assertIsNotNone(target)
            self.assertFalse(bool(target["is_locked"]))


class UserProfileMutationFlowTest(_MutationFlowBase):
    def test_add_admin_mutation_creates_owner_admin_row(self):
        owner_cookie = self.session_cookies["owner_user"]
        owner_token = self.user_tokens["owner_user"]
        csrf_token = self._csrf_for_cookie(owner_cookie)
        request = _make_request(
            self.webhttp,
            "POST",
            f"/users/{urllib.parse.quote(owner_token, safe='')}/admins/add",
            query_string=f"token={urllib.parse.quote(owner_token, safe='')}",
            form_data={
                "csrf_token": csrf_token,
                "username": "helper_user",
            },
            cookie=owner_cookie,
        )

        status, headers, body = self.user_profile_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), f"/users/{urllib.parse.quote(owner_token, safe='')}")

        with self.db.connect_ctx() as conn:
            owner = self._user(conn, "owner_user")
            helper = self._user(conn, "helper_user")
            admins = self.db.list_user_admins(conn, owner["id"])
            self.assertIn(helper["id"], {row["admin_user_id"] for row in admins})

    def test_trust_admin_mutation_updates_trust_flag(self):
        with self.db.connect_ctx() as conn:
            owner = self._user(conn, "owner_user")
            helper = self._user(conn, "helper_user")
            self.db.add_user_admin(conn, owner["id"], helper["id"], trust_admins=False)
        owner_cookie = self.session_cookies["owner_user"]
        owner_token = self.user_tokens["owner_user"]
        csrf_token = self._csrf_for_cookie(owner_cookie)
        request = _make_request(
            self.webhttp,
            "POST",
            f"/users/{urllib.parse.quote(owner_token, safe='')}/admins/trust",
            query_string=f"token={urllib.parse.quote(owner_token, safe='')}",
            form_data={
                "csrf_token": csrf_token,
                "username": "helper_user",
                "trust_admins": "1",
            },
            cookie=owner_cookie,
        )

        status, headers, body = self.user_profile_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), f"/users/{urllib.parse.quote(owner_token, safe='')}")

        with self.db.connect_ctx() as conn:
            owner = self._user(conn, "owner_user")
            helper = self._user(conn, "helper_user")
            admins = self.db.list_user_admins(conn, owner["id"])
            trust_map = {row["admin_user_id"]: row["trust_admins"] for row in admins}
            self.assertTrue(trust_map.get(helper["id"]))

    def test_remove_admin_mutation_deletes_owner_admin_row(self):
        with self.db.connect_ctx() as conn:
            owner = self._user(conn, "owner_user")
            helper = self._user(conn, "helper_user")
            self.db.add_user_admin(conn, owner["id"], helper["id"], trust_admins=True)
        owner_cookie = self.session_cookies["owner_user"]
        owner_token = self.user_tokens["owner_user"]
        csrf_token = self._csrf_for_cookie(owner_cookie)
        request = _make_request(
            self.webhttp,
            "POST",
            f"/users/{urllib.parse.quote(owner_token, safe='')}/admins/remove",
            query_string=f"token={urllib.parse.quote(owner_token, safe='')}",
            form_data={
                "csrf_token": csrf_token,
                "username": "helper_user",
            },
            cookie=owner_cookie,
        )

        status, headers, body = self.user_profile_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), f"/users/{urllib.parse.quote(owner_token, safe='')}")

        with self.db.connect_ctx() as conn:
            owner = self._user(conn, "owner_user")
            admins = self.db.list_user_admins(conn, owner["id"])
            usernames = {row["username"] for row in admins}
            self.assertNotIn("helper_user", usernames)

    def test_add_admin_rejects_invalid_csrf_without_mutation(self):
        owner_cookie = self.session_cookies["owner_user"]
        owner_token = self.user_tokens["owner_user"]
        request = _make_request(
            self.webhttp,
            "POST",
            f"/users/{urllib.parse.quote(owner_token, safe='')}/admins/add",
            query_string=f"token={urllib.parse.quote(owner_token, safe='')}",
            form_data={
                "csrf_token": "invalid-token",
                "username": "helper_user",
            },
            cookie=owner_cookie,
        )

        status, headers, body = self.user_profile_handler.handle(request)
        self.assertEqual(status, "403 Forbidden")
        self.assertTrue(any(key == "Content-Type" for key, _ in headers))
        self.assertIn(b"Forbidden", body)

        with self.db.connect_ctx() as conn:
            owner = self._user(conn, "owner_user")
            admins = self.db.list_user_admins(conn, owner["id"])
            self.assertNotIn("helper_user", {row["username"] for row in admins})

    def test_add_admin_requires_manage_permission_without_mutation(self):
        helper_cookie = self.session_cookies["helper_user"]
        helper_csrf = self._csrf_for_cookie(helper_cookie)
        owner_token = self.user_tokens["owner_user"]
        request = _make_request(
            self.webhttp,
            "POST",
            f"/users/{urllib.parse.quote(owner_token, safe='')}/admins/add",
            query_string=f"token={urllib.parse.quote(owner_token, safe='')}",
            form_data={
                "csrf_token": helper_csrf,
                "username": "extra_user",
            },
            cookie=helper_cookie,
        )

        status, headers, body = self.user_profile_handler.handle(request)
        self.assertEqual(status, "403 Forbidden")
        self.assertTrue(any(key == "Content-Type" for key, _ in headers))
        self.assertIn(b"Forbidden", body)

        with self.db.connect_ctx() as conn:
            owner = self._user(conn, "owner_user")
            admins = self.db.list_user_admins(conn, owner["id"])
            self.assertNotIn("extra_user", {row["username"] for row in admins})


class ServerEditMutationFlowTest(_MutationFlowBase):
    def test_server_edit_mutation_updates_server_and_redirects_to_return_to(self):
        owner_cookie = self.session_cookies["owner_user"]
        owner_token = self.user_tokens["owner_user"]
        server_token = self.server["code"]
        csrf_token = self._csrf_for_cookie(owner_cookie)

        form = {
            "csrf_token": [csrf_token],
            "id": [str(self.server["id"])],
            "host": ["10.20.30.40"],
            "port": ["13999"],
            "name": ["owner-server-renamed"],
            "overview": ["Updated overview"],
            "description": ["Updated description"],
            "owner_username": [""],
            "return_to": [f"/users/{urllib.parse.quote(owner_token, safe='')}"],
        }
        request = _make_request(
            self.webhttp,
            "POST",
            f"/server/{urllib.parse.quote(server_token, safe='')}/edit",
            query_string=f"token={urllib.parse.quote(server_token, safe='')}",
            cookie=owner_cookie,
        )
        request._multipart = (form, {})

        status, headers, body = self.server_edit_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), f"/users/{urllib.parse.quote(owner_token, safe='')}")

        with self.db.connect_ctx() as conn:
            updated = self.db.get_server(conn, self.server["id"])
            self.assertIsNotNone(updated)
            self.assertEqual(updated["host"], "10.20.30.40")
            self.assertEqual(updated["port"], 13999)
            self.assertEqual(updated["name"], "owner-server-renamed")
            self.assertEqual(updated["overview"], "Updated overview")
            self.assertEqual(updated["description"], "Updated description")

    def test_server_delete_mutation_removes_server(self):
        owner_cookie = self.session_cookies["owner_user"]
        csrf_token = self._csrf_for_cookie(owner_cookie)
        request = _make_request(
            self.webhttp,
            "POST",
            "/server/delete",
            form_data={
                "csrf_token": csrf_token,
                "id": str(self.server["id"]),
            },
            cookie=owner_cookie,
        )

        status, headers, body = self.server_edit_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), f"/users/{self.user_tokens['owner_user']}")

        with self.db.connect_ctx() as conn:
            self.assertIsNone(self.db.get_server(conn, self.server["id"]))

    def test_server_edit_rejects_invalid_csrf_without_mutation(self):
        owner_cookie = self.session_cookies["owner_user"]
        server_token = self.server["code"]
        original_host = self.server["host"]
        original_port = self.server["port"]
        request = _make_request(
            self.webhttp,
            "POST",
            f"/server/{urllib.parse.quote(server_token, safe='')}/edit",
            query_string=f"token={urllib.parse.quote(server_token, safe='')}",
            cookie=owner_cookie,
        )
        request._multipart = (
            {
                "csrf_token": ["invalid-token"],
                "id": [str(self.server["id"])],
                "host": ["192.168.50.10"],
                "port": ["14000"],
                "name": ["owner-server-csrf-fail"],
                "overview": ["csrf fail overview"],
                "description": ["csrf fail description"],
                "owner_username": [""],
                "return_to": ["/users/owner_user"],
            },
            {},
        )

        status, headers, body = self.server_edit_handler.handle(request)
        self.assertEqual(status, "403 Forbidden")
        self.assertTrue(any(key == "Content-Type" for key, _ in headers))
        self.assertIn(b"Forbidden", body)

        with self.db.connect_ctx() as conn:
            unchanged = self.db.get_server(conn, self.server["id"])
            self.assertIsNotNone(unchanged)
            self.assertEqual(unchanged["host"], original_host)
            self.assertEqual(unchanged["port"], original_port)

    def test_server_delete_requires_owner_or_admin_without_mutation(self):
        helper_cookie = self.session_cookies["helper_user"]
        helper_csrf = self._csrf_for_cookie(helper_cookie)
        request = _make_request(
            self.webhttp,
            "POST",
            "/server/delete",
            form_data={
                "csrf_token": helper_csrf,
                "id": str(self.server["id"]),
            },
            cookie=helper_cookie,
        )

        status, headers, body = self.server_edit_handler.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertEqual(self._location_header(headers), f"/users/{self.user_tokens['helper_user']}")

        with self.db.connect_ctx() as conn:
            self.assertIsNotNone(self.db.get_server(conn, self.server["id"]))

    def test_server_delete_rejects_invalid_csrf_without_mutation(self):
        owner_cookie = self.session_cookies["owner_user"]
        request = _make_request(
            self.webhttp,
            "POST",
            "/server/delete",
            form_data={
                "csrf_token": "invalid-token",
                "id": str(self.server["id"]),
            },
            cookie=owner_cookie,
        )

        status, headers, body = self.server_edit_handler.handle(request)
        self.assertEqual(status, "403 Forbidden")
        self.assertTrue(any(key == "Content-Type" for key, _ in headers))
        self.assertIn(b"Forbidden", body)

        with self.db.connect_ctx() as conn:
            self.assertIsNotNone(self.db.get_server(conn, self.server["id"]))


if __name__ == "__main__":
    unittest.main()
