#!/usr/bin/env python3
"""Oracle for issue #673: the bug report template must capture the customer
environment specs (issue #673's core demand).

Parses .github/ISSUE_TEMPLATE/bug_report.yml as GitHub issue-form YAML and
requires the five environment fields, each marked strictly required, and all
dropdown options to be plain strings (an unquoted scalar containing ': '
parses as a mapping and GitHub rejects the whole form - council run 9, S2).

--selftest runs the same validation against a deliberately malformed fixture
and must reject it (negative control).

Prints ISSUE673_TEMPLATE_OK / ISSUE673_SELFTEST_OK respectively.
"""
from pathlib import Path
import sys

import yaml

REQUIRED_ENV_FIELDS = {
    "extension-version": "php-firebird extension version",
    "php-version": "PHP version / build / SAPI",
    "client-os": "client OS + architecture",
    "server-version": "Firebird server version",
    "connect-api": "connect API + DSN shape",
}

MALFORMED_FIXTURE = """\
name: Malformed fixture
description: negative control
labels: ["bug"]
body:
  - type: input
    id: extension-version
    attributes:
      label: ext
    validations:
      required: true
  - type: dropdown
    id: connect-api
    attributes:
      label: api
      options:
        - PDO (fbird: DSN)
    validations:
      required: true
"""


def validate_form(form):
    """Return a list of error strings; empty list = valid."""
    errors = []
    if not isinstance(form, dict):
        return ["form is not a mapping"]
    for key in ("name", "description", "body"):
        if key not in form:
            errors.append(f"form key '{key}' missing")
    seen = {}
    for item in form.get("body") or []:
        if not isinstance(item, dict):
            continue
        field_id = item.get("id")
        if item.get("type") == "dropdown":
            for opt in item.get("attributes", {}).get("options") or []:
                if not isinstance(opt, str):
                    errors.append(f"dropdown '{field_id}': non-string option {opt!r}")
        if field_id in REQUIRED_ENV_FIELDS:
            seen[field_id] = item
    missing = sorted(set(REQUIRED_ENV_FIELDS) - set(seen))
    if missing:
        errors.append(f"required environment fields missing: {', '.join(missing)}")
    not_required = sorted(
        k for k, v in seen.items() if v.get("validations", {}).get("required") is not True
    )
    if not_required:
        errors.append(f"fields present but not strictly required: {', '.join(not_required)}")
    return errors


def main() -> int:
    if "--selftest" in sys.argv:
        fixture = yaml.safe_load(MALFORMED_FIXTURE)
        errors = validate_form(fixture)
        if not errors:
            print("selftest FAIL: malformed fixture was accepted (oracle cannot fail)")
            return 1
        print(f"selftest: malformed fixture rejected as expected: {errors[0]}")
        print("ISSUE673_SELFTEST_OK")
        return 0

    form_path = Path(__file__).resolve().parents[2] / ".github" / "ISSUE_TEMPLATE" / "bug_report.yml"
    if not form_path.is_file():
        print("oracle FAIL: .github/ISSUE_TEMPLATE/bug_report.yml missing")
        return 1
    try:
        form = yaml.safe_load(form_path.read_text(encoding="utf-8"))
    except yaml.YAMLError as exc:
        print(f"oracle FAIL: not valid YAML: {exc}")
        return 1
    errors = validate_form(form)
    if errors:
        print("oracle FAIL: template structure invalid:")
        for err in errors:
            print(f"  - {err}")
        return 1
    print(f"oracle info: form '{form['name']}' with {len(form['body'])} body items")
    print("ISSUE673_TEMPLATE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
