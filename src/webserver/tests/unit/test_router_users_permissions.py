import io
import json
import os
import tempfile
import unittest
from unittest import mock


def _load_modules():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if root_dir not in os.sys.path:
        os.sys.path.insert(0, root_dir)
    from karma import auth, config, db, router, webhttp

    return auth, config, db, router, webhttp


def _make_request(webhttp, method, path, cookie=""):
    environ = {
        "REQUEST_METHOD": method,
        "PATH_INFO": path,
        "QUERY_STRING": "",
        "wsgi.input": io.BytesIO(b""),
        "CONTENT_LENGTH": "0",
        "CONTENT_TYPE": "",
        "REMOTE_ADDR": "127.0.0.1",
    }
    if cookie:
        environ["HTTP_COOKIE"] = cookie
    return webhttp.Request(environ)


class RouterUsersPermissionsTest(unittest.TestCase):
    def setUp(self):
        self.auth, self.config, self.db, self.router, self.webhttp = _load_modules()
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

        with self.db.connect_ctx() as conn:
            self.db.add_user(conn, "normal_user", "normal@example.local", "hash-n", "salt-n")
            self.db.add_user(
                conn,
                "admin_user",
                "admin@example.local",
                "hash-a",
                "salt-a",
                is_admin=True,
                is_admin_manual=True,
            )
            normal = self.db.get_user_by_username(conn, "normal_user")
            admin = self.db.get_user_by_username(conn, "admin_user")
            self.assertIsNotNone(normal)
            self.assertIsNotNone(admin)
            self.normal_token = self.auth.sign_user_session(normal["id"])
            self.admin_token = self.auth.sign_user_session(admin["id"])

    def test_users_route_redirects_to_login_when_unauthenticated(self):
        request = _make_request(self.webhttp, "GET", "/users")
        status, headers, body = self.router.dispatch(request)

        self.assertEqual(status, "302 Found")
        self.assertIn(("Location", "/login"), headers)
        self.assertEqual(body, b"")

    def test_users_route_forbids_non_admin_user(self):
        request = _make_request(self.webhttp, "GET", "/users", cookie=f"user_session={self.normal_token}")
        status, headers, body = self.router.dispatch(request)

        self.assertEqual(status, "403 Forbidden")
        self.assertTrue(any(key == "Content-Type" for key, _ in headers))
        self.assertIn(b"Forbidden", body)

    def test_users_route_forwards_to_users_handler_for_admin(self):
        request = _make_request(self.webhttp, "GET", "/users", cookie=f"user_session={self.admin_token}")
        with mock.patch("karma.router.users.handle", return_value=("200 OK", [], b"users-page")) as handler:
            status, headers, body = self.router.dispatch(request)

        self.assertEqual(status, "200 OK")
        self.assertEqual(body, b"users-page")
        handler.assert_called_once_with(request)

    def test_users_profile_route_sets_token_for_user_profile_handler(self):
        request = _make_request(self.webhttp, "GET", "/users/alice")
        with mock.patch("karma.router.user_profile.handle", return_value=("200 OK", [], b"profile")) as handler:
            status, headers, body = self.router.dispatch(request)

        self.assertEqual(status, "200 OK")
        self.assertEqual(body, b"profile")
        self.assertEqual(request.query["token"], ["alice"])
        handler.assert_called_once_with(request)

    def test_users_edit_route_sets_token_for_users_handler(self):
        request = _make_request(self.webhttp, "GET", "/users/alice/edit")
        with mock.patch("karma.router.users.handle", return_value=("200 OK", [], b"edit")) as handler:
            status, headers, body = self.router.dispatch(request)

        self.assertEqual(status, "200 OK")
        self.assertEqual(body, b"edit")
        self.assertEqual(request.query["token"], ["alice"])
        handler.assert_called_once_with(request)

    def test_users_admins_route_sets_token_for_user_profile_handler(self):
        request = _make_request(self.webhttp, "GET", "/users/alice/admins/bob")
        with mock.patch("karma.router.user_profile.handle", return_value=("200 OK", [], b"admins")) as handler:
            status, headers, body = self.router.dispatch(request)

        self.assertEqual(status, "200 OK")
        self.assertEqual(body, b"admins")
        self.assertEqual(request.query["token"], ["alice"])
        handler.assert_called_once_with(request)


if __name__ == "__main__":
    unittest.main()
