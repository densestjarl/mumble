# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.

import subprocess
import sys
import unittest


class ClientTestRunnerTests(unittest.TestCase):
    def invoke(self, *arguments):
        return subprocess.run(
            [runner, *arguments], capture_output=True, text=True, timeout=30
        )

    def test_lists_exactly_the_enabled_suites(self):
        result = self.invoke("--list")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(sorted(result.stdout.splitlines()), sorted(suites))

    def test_requires_a_known_suite(self):
        for arguments in [(), ("UnknownSuite",)]:
            with self.subTest(arguments=arguments):
                result = self.invoke(*arguments)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn("Usage:", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_forwards_qt_arguments_to_each_suite(self):
        for suite in suites:
            with self.subTest(suite=suite):
                result = self.invoke(suite, "-platform", "offscreen", "-functions")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertRegex(result.stdout, r"\w+\(\)")

    def test_preserves_qt_failure_exit_status(self):
        # A nonexistent test function must fail rather than run the whole suite
        # or have its failure swallowed by the dispatcher.
        function = "mumbleNonexistentTestFunction"
        result = self.invoke(suites[0], "-platform", "offscreen", function)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(function, result.stdout + result.stderr)


if __name__ == "__main__":
    runner, *suites = sys.argv[1:]
    unittest.main(argv=[sys.argv[0]])
