from __future__ import annotations

import unittest

from scripts.validate_runtime_identity import REPOSITORY_ROOT, validate_repository


class RuntimeIdentityPolicyTests(unittest.TestCase):
    def test_repository_runtime_identity_policy(self) -> None:
        self.assertEqual(validate_repository(REPOSITORY_ROOT), [])


if __name__ == "__main__":
    unittest.main()
