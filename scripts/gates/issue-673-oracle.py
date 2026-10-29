#!/usr/bin/env python3
"""Oracle for issue #673: the bug report template must capture the customer
environment specs (issue #673's core demand).

Parses .github/ISSUE_TEMPLATE/bug_report.yml as GitHub issue-form YAML and
requires the five environment fields, each marked required. Structure-based
(pyaml), not source-text greps.

Prints ISSUE673_TEMPLATE_OK only if all assertions pass; exits 1 otherwise.
"""
from pathlib import Path

import yaml

REQUIRED_ENV_FIELDS = {
    "extension-version": "php-firebird extension version",
    "php-version": "PHP version / build / SAPI",
    "client-os": "client OS + architecture",
    "server-version": "Firebird server version",
    "connect-api": "connect API + DSN shape",
}

root = Path(__file__).resolve().parents[2]
form_path = root / ".github" / "ISSUE_TEMPLATE" / "bug_report.yml"

if not form_path.is_file():
    print("oracle FAIL: .github/ISSUE_TEMPLATE/bug_report.yml missing")
    raise SystemExit(1)

try:
    form = yaml.safe_load(form_path.read_text(encoding="utf-8"))
except yaml.YAMLError as exc:
    print(f"oracle FAIL: not valid YAML: {exc}")
    raise SystemExit(1)

if not isinstance(form, dict):
    print("oracle FAIL: form is not a mapping")
    raise SystemExit(1)

for key in ("name", "description", "body"):
    if key not in form:
        print(f"oracle FAIL: form key '{key}' missing")
        raise SystemExit(1)

seen = {}
for item in form["body"] or []:
    if not isinstance(item, dict):
        continue
    field_id = item.get("id")
    if field_id in REQUIRED_ENV_FIELDS:
        seen[field_id] = item

missing = sorted(set(REQUIRED_ENV_FIELDS) - set(seen))
if missing:
    print(f"oracle FAIL: required environment fields missing: {', '.join(missing)}")
    raise SystemExit(1)

not_required = sorted(k for k, v in seen.items() if not v.get("validations", {}).get("required"))
if not_required:
    print(f"oracle FAIL: fields present but not required: {', '.join(not_required)}")
    raise SystemExit(1)

labels = form.get("labels") or []
print(f"oracle info: form '{form['name']}' with {len(form['body'])} body items, labels={labels}")
for field_id, item in sorted(seen.items()):
    label = item.get("attributes", {}).get("label", "?")
    print(f"oracle info: {field_id} -> '{label}'")
print("ISSUE673_TEMPLATE_OK")
