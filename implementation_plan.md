# Implementation Plan: Fix Firebird CI Authentication

[Overview]
Resolve the Firebird Docker container authentication issue preventing PHPT tests from connecting to the database in GitHub Actions CI.

The php-firebird CI pipeline currently fails because tests cannot authenticate to Firebird database containers. The error is:
```
FAILED: Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
```

The root causes were identified as:
1.  **Database Path Mismatch (Firebird 2.5)**: `jacobalberty/firebird:2.5-ss` stores db at `/firebird/data/`, while `firebirdsql/firebird:3+` uses `/var/lib/firebird/data/`. The verification script was failing to handle this gracefully.
2.  **WireCrypt/Auth Configuration (Firebird 3.0+)**: The client library requires explicit configuration (`WireCrypt=Disabled`) to connect to servers over the network without complex auth plugin setup.
3.  **Script Logic**: The verification script exited with error code 1 even if a fallback connection succeeded.

**Solution Strategy**:
- Use matrix-conditional `db-path` in `main.yml` to handle path differences.
- Configure `firebird.conf` in the client environment to enable WireCrypt (required by FB3+) and enable legacy auth.
- Fix the verification script logic to exit successfully if a fallback path works.

[Types]
No new type definitions required - this is infrastructure/CI configuration work.

[Files]
Files modified for the CI fix.

**Modified Files:**
1.  `.github/workflows/main.yml` - Added `db-path` to matrix, added client configuration step, fixed verification script.
2.  `.github/workflows/coverage.yml` - Added client configuration step.
3.  `.github/workflows/sanitizers.yml` - Added client configuration step.

[Functions]
No function modifications - this is CI/infrastructure configuration.

[Classes]
No class modifications - this is CI/infrastructure configuration.

[Dependencies]
No new dependencies required.

[Testing]
Manual verification through CI runs.

1.  Push changes to feature branch
2.  Monitor GitHub Actions workflow runs
3.  Verify "Verify extension and connection" step succeeds for all matrix combinations
4.  Verify PHPT tests execute (not skip) for all matrix combinations

[Implementation Order]
Sequential implementation steps to fix the authentication issue.

1.  **Update main.yml workflow**
    -   Add `db-path` to matrix for all combinations.
    -   Add step to configure `firebird.conf` (WireCrypt=Enabled).
    -   Update verification script to handle fallback paths correctly.

2.  **Update coverage.yml workflow**
    -   Add step to configure `firebird.conf`.

3.  **Update sanitizers.yml workflow**
    -   Add step to configure `firebird.conf`.

4.  **Test and verify**
    -   Push changes
    -   Monitor CI results

[Technical Details]
Key configuration settings applied.

**Firebird Client Configuration (`firebird.conf`):**
```ini
# Enable wire encryption (Required by FB3+, ignored by FB2.5)
WireCrypt = Enabled

# Use legacy authentication to match server expectations
AuthClient = Legacy_Auth, Srp, Srp256
```

**Matrix-Conditional Database Paths:**
-   Firebird 2.5: `/firebird/data/test.fdb`
-   Firebird 3.0+: `/var/lib/firebird/data/test.fdb`
