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
    from karma import config, db, webhttp
    from karma.handlers import api

    return config, db, webhttp, api


def _make_request(webhttp, method, path, query_string="", form_data=None):
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
    return webhttp.Request(environ)


class ApiAdminsTest(unittest.TestCase):
    def setUp(self):
        self.config, self.db, self.webhttp, self.api = _load_modules()
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
        with self.db.connect_ctx() as conn:
            users = [
                ("owner_user", "owner@example.local"),
                ("direct_admin", "direct@example.local"),
                ("delegator_trusted", "delegator-trusted@example.local"),
                ("delegated_trusted", "delegated-trusted@example.local"),
                ("delegator_untrusted", "delegator-untrusted@example.local"),
                ("delegated_untrusted", "delegated-untrusted@example.local"),
                ("other_owner", "other-owner@example.local"),
            ]
            self.user_ids = {}
            for username, email in users:
                self.db.add_user(conn, username, email, "hash-" + username, "salt-" + username)
                row = self.db.get_user_by_username(conn, username)
                self.assertIsNotNone(row)
                self.user_ids[username] = row["id"]

            self.db.add_server(
                conn,
                {
                    "name": "alpha",
                    "overview": "alpha overview",
                    "description": "alpha description",
                    "host": "10.0.0.5",
                    "port": 12000,
                    "owner_user_id": self.user_ids["owner_user"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 0,
                    "last_heartbeat": None,
                },
            )
            self.db.add_server(
                conn,
                {
                    "name": "beta",
                    "overview": "beta overview",
                    "description": "beta description",
                    "host": "10.0.0.5",
                    "port": 12001,
                    "owner_user_id": self.user_ids["other_owner"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 0,
                    "last_heartbeat": None,
                },
            )

            self.db.add_user_admin(
                conn,
                self.user_ids["owner_user"],
                self.user_ids["direct_admin"],
                trust_admins=False,
            )
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

    def _decode_json_response(self, response, expected_status):
        status, headers, body = response
        self.assertEqual(status, expected_status)
        self.assertTrue(any(k.lower() == "content-type" for k, _ in headers))
        self.assertIsInstance(body, bytes)
        return json.loads(body.decode("utf-8"))

    def test_get_returns_owner_and_expected_admin_list(self):
        request = _make_request(
            self.webhttp,
            "GET",
            "/api/admins",
            query_string="host=10.0.0.5&port=12000",
        )

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["host"], "10.0.0.5")
        self.assertEqual(payload["port"], 12000)
        self.assertEqual(payload["owner"], "owner_user")
        self.assertEqual(
            payload["admins"],
            [
                "delegated_trusted",
                "delegator_trusted",
                "delegator_untrusted",
                "direct_admin",
            ],
        )

    def test_post_returns_owner_and_expected_admin_list(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/api/admins",
            form_data={"host": "10.0.0.5", "port": "12000"},
        )

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["owner"], "owner_user")

    def test_rejects_non_get_post_method(self):
        request = _make_request(
            self.webhttp,
            "PUT",
            "/api/admins",
            form_data={"host": "10.0.0.5", "port": "12000"},
        )

        payload = self._decode_json_response(self.api.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_requires_host(self):
        request = _make_request(self.webhttp, "GET", "/api/admins", query_string="port=12000")

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "missing_host")

    def test_requires_port(self):
        request = _make_request(self.webhttp, "GET", "/api/admins", query_string="host=10.0.0.5")

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "missing_port")

    def test_rejects_invalid_port(self):
        request = _make_request(self.webhttp, "GET", "/api/admins", query_string="host=10.0.0.5&port=abc")

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_port")

    def test_returns_host_not_found(self):
        request = _make_request(self.webhttp, "GET", "/api/admins", query_string="host=10.0.0.99&port=12000")

        payload = self._decode_json_response(self.api.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "host_not_found")

    def test_returns_port_not_found_when_host_exists(self):
        request = _make_request(self.webhttp, "GET", "/api/admins", query_string="host=10.0.0.5&port=12999")

        payload = self._decode_json_response(self.api.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "port_not_found")

    def test_returns_owner_not_found_when_owner_deleted(self):
        with self.db.connect_ctx() as conn:
            self.db.set_user_deleted(conn, self.user_ids["owner_user"], True)

        request = _make_request(
            self.webhttp,
            "GET",
            "/api/admins",
            query_string="host=10.0.0.5&port=12000",
        )

        payload = self._decode_json_response(self.api.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "owner_not_found")


if __name__ == "__main__":
    unittest.main()
