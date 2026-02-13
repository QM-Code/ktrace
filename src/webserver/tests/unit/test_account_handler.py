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
    from karma.handlers import account

    return auth, config, db, webhttp, account


def _make_request(webhttp, method, path, query_string="", form_data=None, extra_environ=None):
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
    if extra_environ:
        environ.update(extra_environ)
    return webhttp.Request(environ)


class AccountHandlerTest(unittest.TestCase):
    def setUp(self):
        self.auth, self.config, self.db, self.webhttp, self.account = _load_modules()
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

    def test_register_get_renders_form(self):
        request = _make_request(self.webhttp, "GET", "/register")
        status, headers, body = self.account.handle(request)

        self.assertEqual(status, "200 OK")
        self.assertTrue(any(k == "Content-Type" for k, _ in headers))
        self.assertIn(b"action=\"/register\"", body)

    def test_login_get_renders_form(self):
        request = _make_request(self.webhttp, "GET", "/login")
        status, headers, body = self.account.handle(request)

        self.assertEqual(status, "200 OK")
        self.assertIn(b"action=\"/login\"", body)

    def test_register_put_rejected(self):
        request = _make_request(self.webhttp, "PUT", "/register")
        status, headers, body = self.account.handle(request)

        self.assertEqual(status, "405 Method Not Allowed")
        self.assertTrue(any(k == "Content-Type" for k, _ in headers))
        self.assertIn(b"Method Not Allowed", body)

    def test_register_post_requires_csrf(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/register",
            form_data={"username": "u1", "email": "u1@example.local", "password": "pw"},
        )
        status, headers, body = self.account.handle(request)

        self.assertEqual(status, "403 Forbidden")
        self.assertTrue(any(k == "Content-Type" for k, _ in headers))
        self.assertIn(b"Forbidden", body)

    def test_login_post_requires_csrf(self):
        request = _make_request(
            self.webhttp,
            "POST",
            "/login",
            form_data={"email": "u1@example.local", "password": "pw"},
        )
        status, headers, body = self.account.handle(request)

        self.assertEqual(status, "403 Forbidden")
        self.assertIn(b"Forbidden", body)

    def test_logout_get_clears_session_cookie_and_redirects(self):
        request = _make_request(self.webhttp, "GET", "/logout")
        status, headers, body = self.account.handle(request)

        self.assertEqual(status, "302 Found")
        self.assertIn(("Location", "/servers"), headers)
        cookie_headers = [value for key, value in headers if key == "Set-Cookie"]
        self.assertTrue(any("user_session=expired" in value for value in cookie_headers))
        self.assertEqual(body, b"")

    def test_register_post_success_creates_user_and_redirects(self):
        csrf_token = "test-csrf"
        request = _make_request(
            self.webhttp,
            "POST",
            "/register",
            form_data={
                "csrf_token": csrf_token,
                "username": "new_user",
                "email": "new_user@example.local",
                "password": "pw-new-user",
            },
            extra_environ={"KARMA_CSRF_TOKEN": csrf_token},
        )

        status, headers, body = self.account.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")

        cookie_headers = [value for key, value in headers if key == "Set-Cookie"]
        self.assertTrue(any("user_session=" in value for value in cookie_headers))
        self.assertTrue(any(key == "Location" and value.startswith("/") for key, value in headers))

        with self.db.connect_ctx() as conn:
            created = self.db.get_user_by_username(conn, "new_user")
            self.assertIsNotNone(created)

    def test_login_post_success_sets_session_cookie_and_redirects(self):
        with self.db.connect_ctx() as conn:
            password_hash, password_salt = self.auth.new_password("pw-existing")
            self.db.add_user(conn, "existing_user", "existing@example.local", password_hash, password_salt)

        csrf_token = "login-csrf"
        request = _make_request(
            self.webhttp,
            "POST",
            "/login",
            form_data={
                "csrf_token": csrf_token,
                "email": "existing@example.local",
                "password": "pw-existing",
            },
            extra_environ={"KARMA_CSRF_TOKEN": csrf_token},
        )

        status, headers, body = self.account.handle(request)
        self.assertEqual(status, "302 Found")
        self.assertEqual(body, b"")
        self.assertTrue(any(key == "Set-Cookie" and "user_session=" in value for key, value in headers))
        self.assertTrue(any(key == "Location" and value.startswith("/") for key, value in headers))

    def test_unknown_path_returns_not_found(self):
        request = _make_request(self.webhttp, "GET", "/not-a-real-account-route")
        status, headers, body = self.account.handle(request)

        self.assertEqual(status, "404 Not Found")
        self.assertTrue(any(k == "Content-Type" for k, _ in headers))
        self.assertIn(b"Not Found", body)


if __name__ == "__main__":
    unittest.main()
