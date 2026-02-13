import io
import json
import os
import tempfile
import time
import unittest
import urllib.parse


def _load_modules():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if root_dir not in os.sys.path:
        os.sys.path.insert(0, root_dir)
    from karma import config, db, webhttp
    from karma.handlers import api_user

    return config, db, webhttp, api_user


def _make_request(webhttp, method, path, query_params=None):
    query_string = ""
    if query_params:
        query_string = urllib.parse.urlencode(query_params)
    environ = {
        "REQUEST_METHOD": method,
        "PATH_INFO": path,
        "QUERY_STRING": query_string,
        "wsgi.input": io.BytesIO(b""),
        "CONTENT_LENGTH": "0",
        "CONTENT_TYPE": "",
        "REMOTE_ADDR": "127.0.0.1",
    }
    return webhttp.Request(environ)


class ApiUserTest(unittest.TestCase):
    def setUp(self):
        self.config, self.db, self.webhttp, self.api_user = _load_modules()
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
            self.db.add_user(conn, "owner_a", "owner_a@example.local", "hash-a", "salt-a")
            owner = self.db.get_user_by_username(conn, "owner_a")
            self.assertIsNotNone(owner)
            self.owner_id = owner["id"]
            self.owner_code = owner["code"]

            self.db.add_user(conn, "deleted_owner", "deleted_owner@example.local", "hash-d", "salt-d")
            deleted_owner = self.db.get_user_by_username(conn, "deleted_owner")
            self.assertIsNotNone(deleted_owner)
            self.db.set_user_deleted(conn, deleted_owner["id"], True)

            now = int(time.time())
            long_overview = "o" * 200
            self.db.add_server(
                conn,
                {
                    "name": "alpha",
                    "overview": long_overview,
                    "description": "alpha description",
                    "host": "10.0.0.10",
                    "port": 11901,
                    "owner_user_id": self.owner_id,
                    "screenshot_id": "shot-alpha",
                    "max_players": 20,
                    "num_players": 5,
                    "last_heartbeat": now,
                },
            )
            self.db.add_server(
                conn,
                {
                    "name": "beta",
                    "overview": "beta overview",
                    "description": "beta description",
                    "host": "10.0.0.11",
                    "port": 11902,
                    "owner_user_id": self.owner_id,
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 1,
                    "last_heartbeat": now - 1000,
                },
            )

    def _decode_json_response(self, response, expected_status):
        status, headers, body = response
        self.assertEqual(status, expected_status)
        self.assertTrue(any(k.lower() == "content-type" for k, _ in headers))
        self.assertIsInstance(body, bytes)
        return json.loads(body.decode("utf-8"))

    def test_rejects_non_get_method(self):
        request = _make_request(self.webhttp, "POST", "/api/user")

        payload = self._decode_json_response(self.api_user.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_requires_lookup_identifier(self):
        request = _make_request(self.webhttp, "GET", "/api/user")

        payload = self._decode_json_response(self.api_user.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "missing_name")

    def test_lookup_by_username_returns_user_and_servers(self):
        request = _make_request(self.webhttp, "GET", "/api/user", {"name": "owner_a"})

        payload = self._decode_json_response(self.api_user.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["user"]["username"], "owner_a")
        self.assertEqual(payload["active_count"], 1)
        self.assertEqual(payload["inactive_count"], 1)
        self.assertEqual(len(payload["servers"]), 2)

        # Sorted by num_players descending.
        self.assertEqual(payload["servers"][0]["name"], "alpha")
        self.assertEqual(payload["servers"][0]["num_players"], 5)
        self.assertTrue(payload["servers"][0]["active"])
        self.assertEqual(len(payload["servers"][0]["overview"]), 160)

    def test_lookup_by_code_returns_user_and_servers(self):
        request = _make_request(self.webhttp, "GET", "/api/user", {"code": self.owner_code})

        payload = self._decode_json_response(self.api_user.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["user"]["code"], self.owner_code)

    def test_lookup_rejects_deleted_user(self):
        request = _make_request(self.webhttp, "GET", "/api/user", {"name": "deleted_owner"})

        payload = self._decode_json_response(self.api_user.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "not_found")

    def test_lookup_returns_not_found_for_unknown_user(self):
        request = _make_request(self.webhttp, "GET", "/api/user", {"name": "missing"})

        payload = self._decode_json_response(self.api_user.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "not_found")


if __name__ == "__main__":
    unittest.main()
