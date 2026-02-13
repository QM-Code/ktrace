import io
import json
import os
import tempfile
import time
import unittest


def _load_modules():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if root_dir not in os.sys.path:
        os.sys.path.insert(0, root_dir)
    from karma import config, db, webhttp
    from karma.handlers import api_servers
    return config, db, webhttp, api_servers


def _make_request(webhttp, path, query_string=""):
    environ = {
        "REQUEST_METHOD": "GET",
        "PATH_INFO": path,
        "QUERY_STRING": query_string,
        "wsgi.input": io.BytesIO(b""),
        "CONTENT_LENGTH": "0",
        "CONTENT_TYPE": "",
        "REMOTE_ADDR": "127.0.0.1",
    }
    return webhttp.Request(environ)


class ApiServersActiveTest(unittest.TestCase):
    def setUp(self):
        self.config, self.db, self.webhttp, self.api_servers = _load_modules()
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
            self.db.add_user(conn, "owner_b", "owner_b@example.local", "hash-b", "salt-b")

            owner_a = self.db.get_user_by_username(conn, "owner_a")
            owner_b = self.db.get_user_by_username(conn, "owner_b")
            self.assertIsNotNone(owner_a)
            self.assertIsNotNone(owner_b)

            now = int(time.time())
            records = [
                {
                    "name": "alpha",
                    "overview": "a" * 220,
                    "description": "alpha description",
                    "host": "10.0.0.10",
                    "port": 11901,
                    "owner_user_id": owner_a["id"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 3,
                    "last_heartbeat": now - 10,
                },
                {
                    "name": "beta",
                    "overview": "beta overview",
                    "description": "beta description",
                    "host": "10.0.0.11",
                    "port": 11902,
                    "owner_user_id": owner_b["id"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 8,
                    "last_heartbeat": now - 30,
                },
                {
                    "name": "gamma",
                    "overview": "gamma overview",
                    "description": "gamma description",
                    "host": "10.0.0.12",
                    "port": 11903,
                    "owner_user_id": owner_a["id"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 1,
                    "last_heartbeat": now - 400,
                },
            ]
            for record in records:
                self.db.add_server(conn, record)

    def _decode_json_response(self, response, expected_status):
        status, headers, body = response
        self.assertEqual(status, expected_status)
        self.assertTrue(any(k.lower() == "content-type" for k, _ in headers))
        self.assertIsInstance(body, bytes)
        return json.loads(body.decode("utf-8"))

    def test_active_list_counts_and_sorting(self):
        request = _make_request(self.webhttp, "/api/servers/active")
        payload = self._decode_json_response(self.api_servers.handle(request, status="active"), "200 OK")

        self.assertEqual(payload["community_name"], "Unit Test Community")
        self.assertEqual(payload["active_count"], 2)
        self.assertEqual(payload["inactive_count"], 1)
        self.assertEqual(len(payload["servers"]), 2)

        # Active list is sorted by num_players descending for non-owner-scoped list calls.
        self.assertEqual(payload["servers"][0]["name"], "beta")
        self.assertEqual(payload["servers"][0]["num_players"], 8)
        self.assertEqual(payload["servers"][1]["name"], "alpha")
        self.assertEqual(payload["servers"][1]["num_players"], 3)
        self.assertEqual(len(payload["servers"][1]["overview"]), 160)

    def test_owner_filter_limits_results(self):
        request = _make_request(self.webhttp, "/api/servers", "owner=owner_a")
        payload = self._decode_json_response(self.api_servers.handle(request, status="all"), "200 OK")

        self.assertEqual(payload["active_count"], 1)
        self.assertEqual(payload["inactive_count"], 1)
        returned_names = sorted(item["name"] for item in payload["servers"])
        self.assertEqual(returned_names, ["alpha", "gamma"])

    def test_rejects_non_get_method(self):
        environ = {
            "REQUEST_METHOD": "POST",
            "PATH_INFO": "/api/servers",
            "QUERY_STRING": "",
            "wsgi.input": io.BytesIO(b""),
            "CONTENT_LENGTH": "0",
            "CONTENT_TYPE": "",
            "REMOTE_ADDR": "127.0.0.1",
        }
        request = self.webhttp.Request(environ)

        payload = self._decode_json_response(self.api_servers.handle(request, status="all"), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_rejects_invalid_status(self):
        request = _make_request(self.webhttp, "/api/servers")
        payload = self._decode_json_response(self.api_servers.handle(request, status="broken"), "400 Bad Request")

        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_status")

    def test_owner_filter_returns_user_not_found(self):
        request = _make_request(self.webhttp, "/api/servers", "owner=missing_owner")
        payload = self._decode_json_response(self.api_servers.handle(request, status="all"), "404 Not Found")

        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "user_not_found")


if __name__ == "__main__":
    unittest.main()
