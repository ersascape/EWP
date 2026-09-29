import importlib.util
import pathlib
import unittest


MODULE_PATH = pathlib.Path(__file__).parents[1] / "scripts" / "ewctl.py"
SPEC = importlib.util.spec_from_file_location("ewctl", MODULE_PATH)
ewctl = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ewctl)


class ProtocolTests(unittest.TestCase):
    def test_request_is_versioned_and_newline_terminated(self):
        frame = ewctl.encode_request(7, "battery.read", {"fresh": True})
        self.assertTrue(frame.endswith(b"\n"))
        self.assertEqual(
            ewctl.decode_response(frame),
            {"v": 1, "id": 7, "cmd": "battery.read", "args": {"fresh": True}},
        )

    def test_rejects_multiline_or_empty_command(self):
        for command in ("", "a\nb", "a\rb"):
            with self.subTest(command=command), self.assertRaises(ewctl.EwctlError):
                ewctl.encode_request(1, command)

    def test_rejects_oversized_request(self):
        with self.assertRaises(ewctl.EwctlError):
            ewctl.encode_request(1, "x", {"data": "z" * ewctl.MAX_FRAME_BYTES})

    def test_invalid_or_non_object_reply_is_ignored(self):
        self.assertIsNone(ewctl.decode_response(b"boot log text\n"))
        self.assertIsNone(ewctl.decode_response(b"[]\n"))
        self.assertIsNone(ewctl.decode_response(b"x" * (ewctl.MAX_FRAME_BYTES + 1)))

    def test_args_must_be_json_object(self):
        self.assertEqual(ewctl.parse_args_json('{"limit":10}'), {"limit": 10})
        for text in ("[1,2]", '"text"', "no-json"):
            with self.subTest(text=text), self.assertRaises(ewctl.EwctlError):
                ewctl.parse_args_json(text)

    def test_power_frequency_commands_have_cli_forms(self):
        parser = ewctl.build_parser()
        set_args = parser.parse_args(["power", "cpu-freq-set", "40"])
        self.assertEqual((set_args.operation, set_args.power_action, set_args.mhz), ("power", "cpu-freq-set", "40"))
        get_args = parser.parse_args(["power", "cpu-freq-get"])
        self.assertEqual((get_args.operation, get_args.power_action), ("power", "cpu-freq-get"))
        self.assertTrue(parser.parse_args(["--json", "power"]).json)
        self.assertTrue(parser.parse_args(["power", "cpu-freq-get", "--json"]).json)


if __name__ == "__main__":
    unittest.main()
