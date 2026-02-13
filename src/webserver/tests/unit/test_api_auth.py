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
    from karma.handlers import api

    return auth, config, db, webhttp, api


def _make_request(webhttp, method, path, query_string="", form_data=None, remote_addr="127.0.0.1"):
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
        "REMOTE_ADDR": remote_addr,
    }
    return webhttp.Request(environ)


class ApiAuthTest(unittest.TestCase):
    def setUp(self):
        self.auth, self.config, self.db, self.webhttp, self.api = _load_modules()
        self.temp_dir = tempfile.TemporaryDirectory(prefix="karma-unit-community-")
        self.addCleanup(self.temp_dir.cleanup)

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
        self._seed_data()

    def _seed_data(self):
        self.user_hashes = {}
        self.user_ids = {}

        with self.db.connect_ctx() as conn:
            self._add_user(conn, "registered_user", "registered@example.local")
            self._add_user(conn, "community_admin_user", "community-admin@example.local", is_admin=True)
            self._add_user(conn, "owner_user", "owner@example.local")
            self._add_user(conn, "direct_admin", "direct-admin@example.local")
            self._add_user(conn, "delegator_trusted", "delegator-trusted@example.local")
            self._add_user(conn, "delegated_trusted", "delegated-trusted@example.local")
            self._add_user(conn, "delegator_untrusted", "delegator-untrusted@example.local")
            self._add_user(conn, "delegated_untrusted", "delegated-untrusted@example.local")
            self._add_user(conn, "locked_user", "locked@example.local")
            self._add_user(conn, "deleted_user", "deleted@example.local")

            self.db.add_server(
                conn,
                {
                    "name": "world-alpha",
                    "overview": "world alpha",
                    "description": "world alpha description",
                    "host": "127.0.0.1",
                    "port": 12999,
                    "owner_user_id": self.user_ids["owner_user"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 0,
                    "last_heartbeat": None,
                },
            )

            # Direct admin of owner.
            self.db.add_user_admin(
                conn,
                self.user_ids["owner_user"],
                self.user_ids["direct_admin"],
                trust_admins=False,
            )

            # Trusted delegation chain: owner -> delegator_trusted (trust enabled) -> delegated_trusted.
            self.db.add_user_admin(
                conn,
                self.user_ids["owner_user"],
                self.user_ids["delegator_trusted"],
                trust_admins=True,
            )
            self.db.add_user_admin(
                conn,
                self.user_ids["delegator_trusted"],
                self.user_ids["delegated_trusted"],
                trust_admins=False,
            )

            # Untrusted delegation chain: owner -> delegator_untrusted (trust disabled) -> delegated_untrusted.
            self.db.add_user_admin(
                conn,
                self.user_ids["owner_user"],
                self.user_ids["delegator_untrusted"],
                trust_admins=False,
            )
            self.db.add_user_admin(
                conn,
                self.user_ids["delegator_untrusted"],
                self.user_ids["delegated_untrusted"],
                trust_admins=False,
            )

            self.db.set_user_locked(conn, self.user_ids["locked_user"], True)
            self.db.set_user_deleted(conn, self.user_ids["deleted_user"], True)

    def _add_user(self, conn, username, email, is_admin=False):
        password_hash, password_salt = self.auth.new_password("pw-" + username)
        self.db.add_user(conn, username, email, password_hash, password_salt, is_admin=is_admin, is_admin_manual=is_admin)
        user = self.db.get_user_by_username(conn, username)
        self.assertIsNotNone(user)
        self.user_hashes[username] = password_hash
        self.user_ids[username] = user["id"]

    def _decode_json_response(self, response, expected_status):
        status, headers, body = response
        self.assertEqual(status, expected_status)
        self.assertTrue(any(k.lower() == "content-type" for k, _ in headers))
        self.assertIsInstance(body, bytes)
        return json.loads(body.decode("utf-8"))

    def _auth_request(self, username, expected_status, world=""):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/auth",
            form_data={
                "username": username,
                "passhash": self.user_hashes.get(username, "deadbeef"),
                "world": world,
            },
        )
        return self._decode_json_response(self.api.handle(request), expected_status)

    def test_user_registered_returns_registered_true_with_salt(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/user_registered",
            form_data={"username": "registered_user"},
        )

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertTrue(payload["registered"])
        self.assertEqual(payload["community_name"], "Unit Test Community")
        with self.db.connect_ctx() as conn:
            user = self.db.get_user_by_username(conn, "registered_user")
            self.assertIsNotNone(user)
            self.assertEqual(payload["salt"], user["password_salt"])
        self.assertFalse(payload["locked"])
        self.assertFalse(payload["deleted"])

    def test_user_registered_returns_registered_false_for_unknown_username(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/user_registered",
            form_data={"username": "not_registered"},
        )

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertFalse(payload["registered"])
        self.assertEqual(payload["community_name"], "Unit Test Community")
        self.assertNotIn("salt", payload)

    def test_auth_accepts_registered_user_with_valid_passhash(self):
        payload = self._auth_request("registered_user", "200 OK")
        self.assertTrue(payload["ok"])
        self.assertFalse(payload["community_admin"])
        self.assertFalse(payload["local_admin"])

    def test_auth_rejects_registered_user_with_invalid_passhash(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/auth",
            form_data={
                "username": "registered_user",
                "passhash": "deadbeef",
            },
        )

        payload = self._decode_json_response(self.api.handle(request), "401 Unauthorized")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_credentials")

    def test_auth_rejects_unregistered_username(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/auth",
            form_data={
                "username": "missing_user",
                "passhash": self.user_hashes["registered_user"],
            },
        )

        payload = self._decode_json_response(self.api.handle(request), "401 Unauthorized")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_credentials")

    def test_auth_rejects_get_when_debug_auth_disabled(self):
        request = _make_request(
            self.webhttp,
            "GET",
            "/api/auth",
            query_string="username=registered_user&passhash=" + self.user_hashes["registered_user"],
        )

        payload = self._decode_json_response(self.api.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_auth_rejects_non_get_post_method(self):
        request = _make_request(
            self.webhttp,
            "PUT",
            "/api/auth",
            form_data={"username": "registered_user", "passhash": self.user_hashes["registered_user"]},
        )

        payload = self._decode_json_response(self.api.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_auth_requires_password_or_passhash(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/auth",
            form_data={"username": "registered_user"},
        )

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "missing_credentials")

    def test_auth_requires_username_or_email(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/auth",
            form_data={"passhash": self.user_hashes["registered_user"]},
        )

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "missing_credentials")

    def test_auth_accepts_email_lookup(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/auth",
            form_data={
                "email": "registered@example.local",
                "passhash": self.user_hashes["registered_user"],
            },
        )

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertTrue(payload["ok"])

    def test_auth_rejects_locked_user(self):
        payload = self._auth_request("locked_user", "401 Unauthorized")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_credentials")

    def test_auth_rejects_deleted_user(self):
        payload = self._auth_request("deleted_user", "401 Unauthorized")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_credentials")

    def test_auth_sets_community_admin_true_for_admin_user(self):
        payload = self._auth_request("community_admin_user", "200 OK")
        self.assertTrue(payload["ok"])
        self.assertTrue(payload["community_admin"])

    def test_auth_get_allowed_when_debug_auth_enabled(self):
        settings = self.config.get_config()
        original = bool(settings["debug"]["auth"])
        settings["debug"]["auth"] = True
        try:
            request = _make_request(
                self.webhttp,
                "GET",
                "/api/auth",
                query_string=(
                    "username=registered_user&passhash="
                    + self.user_hashes["registered_user"]
                ),
            )
            payload = self._decode_json_response(self.api.handle(request), "200 OK")
            self.assertTrue(payload["ok"])
        finally:
            settings["debug"]["auth"] = original

    def test_user_registered_requires_username(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/user_registered",
            form_data={},
        )

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "missing_username")

    def test_user_registered_rejects_non_get_post_method(self):
        request = _make_request(
            self.webhttp,
            "PUT",
            "/api/user_registered",
            form_data={"username": "registered_user"},
        )

        payload = self._decode_json_response(self.api.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_auth_world_owner_is_local_admin(self):
        payload = self._auth_request("owner_user", "200 OK", world="world-alpha")
        self.assertTrue(payload["ok"])
        self.assertTrue(payload["local_admin"])

    def test_auth_world_direct_admin_is_local_admin(self):
        payload = self._auth_request("direct_admin", "200 OK", world="world-alpha")
        self.assertTrue(payload["ok"])
        self.assertTrue(payload["local_admin"])

    def test_auth_world_trusted_delegated_admin_is_local_admin(self):
        payload = self._auth_request("delegated_trusted", "200 OK", world="world-alpha")
        self.assertTrue(payload["ok"])
        self.assertTrue(payload["local_admin"])

    def test_auth_world_untrusted_delegated_admin_is_not_local_admin(self):
        payload = self._auth_request("delegated_untrusted", "200 OK", world="world-alpha")
        self.assertTrue(payload["ok"])
        self.assertFalse(payload["local_admin"])

    def test_auth_world_local_admin_is_false_for_unknown_world(self):
        payload = self._auth_request("owner_user", "200 OK", world="world-missing")
        self.assertTrue(payload["ok"])
        self.assertFalse(payload["local_admin"])


if __name__ == "__main__":
    unittest.main()
