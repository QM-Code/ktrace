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
    from karma.handlers import api_server

    return config, db, webhttp, api_server


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


class ApiServerTest(unittest.TestCase):
    def setUp(self):
        self.config, self.db, self.webhttp, self.api_server = _load_modules()
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

            now = int(time.time())
            self.db.add_server(
                conn,
                {
                    "name": "alpha",
                    "overview": "alpha overview",
                    "description": "alpha description",
                    "host": "10.0.0.10",
                    "port": 11901,
                    "owner_user_id": owner["id"],
                    "screenshot_id": "shot-alpha",
                    "max_players": 20,
                    "num_players": 3,
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
                    "owner_user_id": owner["id"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 0,
                    "last_heartbeat": now - 1000,
                },
            )

            alpha = self.db.get_server_by_name(conn, "alpha")
            beta = self.db.get_server_by_name(conn, "beta")
            self.assertIsNotNone(alpha)
            self.assertIsNotNone(beta)
            self.alpha_id = alpha["id"]
            self.alpha_code = alpha["code"]
            self.beta_id = beta["id"]

    def _decode_json_response(self, response, expected_status):
        status, headers, body = response
        self.assertEqual(status, expected_status)
        self.assertTrue(any(k.lower() == "content-type" for k, _ in headers))
        self.assertIsInstance(body, bytes)
        return json.loads(body.decode("utf-8"))

    def test_rejects_non_get_method(self):
        request = _make_request(self.webhttp, "POST", "/api/server")

        payload = self._decode_json_response(self.api_server.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_requires_lookup_identifier(self):
        request = _make_request(self.webhttp, "GET", "/api/server")

        payload = self._decode_json_response(self.api_server.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "missing_name")

    def test_rejects_invalid_id_query(self):
        request = _make_request(self.webhttp, "GET", "/api/server", {"id": "abc"})

        payload = self._decode_json_response(self.api_server.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_id")

    def test_lookup_by_code_returns_server_payload(self):
        request = _make_request(self.webhttp, "GET", "/api/server", {"code": self.alpha_code})

        payload = self._decode_json_response(self.api_server.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["server"]["id"], self.alpha_id)
        self.assertEqual(payload["server"]["name"], "alpha")
        self.assertTrue(payload["server"]["active"])
        self.assertEqual(payload["server"]["owner"], "owner_a")

    def test_lookup_by_name_returns_server_payload(self):
        request = _make_request(self.webhttp, "GET", "/api/server", {"name": "alpha"})

        payload = self._decode_json_response(self.api_server.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["server"]["name"], "alpha")

    def test_lookup_by_id_returns_inactive_server(self):
        request = _make_request(self.webhttp, "GET", "/api/server", {"id": str(self.beta_id)})

        payload = self._decode_json_response(self.api_server.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["server"]["id"], self.beta_id)
        self.assertFalse(payload["server"]["active"])

    def test_lookup_by_numeric_token_falls_back_to_id(self):
        request = _make_request(self.webhttp, "GET", "/api/server", {"token": str(self.beta_id)})

        payload = self._decode_json_response(self.api_server.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["server"]["id"], self.beta_id)

    def test_returns_not_found_for_unknown_lookup(self):
        request = _make_request(self.webhttp, "GET", "/api/server", {"name": "missing"})

        payload = self._decode_json_response(self.api_server.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "not_found")


if __name__ == "__main__":
    unittest.main()
