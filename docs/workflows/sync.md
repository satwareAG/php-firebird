# Syncing with Upstream Repository

This workflow explains how to incorporate changes from the upstream repository into our fork.

## Updating the Upstream Mirror

```bash
# Fetch latest changes from upstream
git checkout upstream-mirror
git fetch upstream
git reset --hard upstream/master
git push --force origin upstream-mirror
```

Or in IntelliJ IDEA:
1. Switch to `upstream-mirror` branch via Git dropdown
2. Select **Git → Pull**
3. Choose `upstream` as remote and `master` as branch
4. Click **Pull**
5. Select **Git → Push** and force push to origin

## Cherry-picking Specific Changes

Identify specific upstream commits to incorporate:

```bash
# View upstream commits
git log upstream-mirror
# Note the commit hash of changes to include

# Apply to satware-main
git checkout satware-main
git cherry-pick <commit-hash>
git push origin satware-main
```

Or in IntelliJ IDEA:
1. Switch to `satware-main` branch
2. Open **Git Log** tab
3. Locate desired commit on `upstream-mirror`
4. Right-click and select **Cherry-pick**
5. Push changes