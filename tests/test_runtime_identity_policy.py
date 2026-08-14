from __future__ import annotations

import copy
import json
import unittest

from scripts.validate_runtime_identity import (
    IDENTITY_PATH,
    REPOSITORY_ROOT,
    validate_identity,
    validate_repository,
)


class RuntimeIdentityPolicyTests(unittest.TestCase):
    def test_repository_runtime_identity_policy(self) -> None:
        self.assertEqual(validate_repository(REPOSITORY_ROOT), [])

    def test_unapproved_build_class_label_is_rejected(self) -> None:
        identity = json.loads((REPOSITORY_ROOT / IDENTITY_PATH).read_text(encoding="utf-8"))
        mutated = copy.deepcopy(identity)
        mutated["buildClasses"]["release"]["label"] = "Supported downstream release"

        errors = validate_identity(mutated)

        self.assertTrue(any("buildClasses.release.label" in error for error in errors))

    def test_official_status_in_manifest_is_rejected(self) -> None:
        identity = json.loads((REPOSITORY_ROOT / IDENTITY_PATH).read_text(encoding="utf-8"))
        mutated = copy.deepcopy(identity)
        mutated["buildClasses"]["test"]["label"] = "Official Community Mod"

        errors = validate_identity(mutated)

        self.assertTrue(any("forbidden runtime branding" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
