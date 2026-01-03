# Packagist Registration Procedure

This document describes the procedure for registering `satwareag/php-firebird-stubs` on Packagist.org.

## Prerequisites

1. **GitHub repository exists**: `satwareAG/php-firebird-stubs` ✅
2. **Repository has valid `composer.json`** at root (populated by mono-repo split action)
3. **Packagist account** with organization access

## Step 1: Create Fine-Grained GitHub PAT for Mono-Repo Split

The GitHub Actions workflow needs a Personal Access Token to push to the stubs repository.

### Create at: https://github.com/settings/tokens?type=beta

**Settings:**
- **Token name**: `php-firebird-stubs-split`
- **Expiration**: 90 days (set reminder to rotate)
- **Resource owner**: `satwareAG`
- **Repository access**: `Only select repositories` → `satwareAG/php-firebird-stubs`
- **Permissions**:
  - **Contents**: Read and write
  - **Metadata**: Read (automatically granted)

**Copy the token value** - you'll need it in Step 2.

## Step 2: Add PAT as GitHub Secret

### Via CLI:
```bash
gh secret set STUBS_REPO_TOKEN --repo satwareAG/php-firebird
# Paste the token when prompted
```

### Via Web UI:
1. Go to: https://github.com/satwareAG/php-firebird/settings/secrets/actions
2. Click "New repository secret"
3. Name: `STUBS_REPO_TOKEN`
4. Value: (paste the fine-grained PAT from Step 1)
5. Click "Add secret"

## Step 3: Trigger Initial Split

Once the secret is configured, trigger the split workflow:

```bash
# Push to main branch (if stubs/ changed)
git push origin main

# Or manually trigger the workflow
gh workflow run split-stubs.yml --repo satwareAG/php-firebird
```

**Verify**: Check https://github.com/satwareAG/php-firebird-stubs to see the populated content.

## Step 4: Register on Packagist via API

### Get Your Packagist API Token

1. Go to: https://packagist.org/profile/
2. Find "API Tokens" section
3. Copy the **Main Token** (not the "Safe Token")
   - Main Token is required for package creation (write operation)

### Register via curl

```bash
# Replace YOUR_USERNAME and YOUR_MAIN_API_TOKEN
curl -X POST \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer YOUR_USERNAME:YOUR_MAIN_API_TOKEN' \
  'https://packagist.org/api/create-package' \
  -d '{"repository":"https://github.com/satwareAG/php-firebird-stubs"}'
```

**Expected response:**
```json
{"status": "success"}
```

## Step 5: Configure Auto-Update Webhook (Optional)

Packagist auto-updates packages, but you can configure a webhook for immediate updates:

### Via Packagist UI:
1. Go to: https://packagist.org/packages/satwareag/php-firebird-stubs
2. Click the package settings/webhook button
3. Follow instructions to add webhook to GitHub

### Via GitHub API:
The mono-repo split action automatically pushes to the target repo, which triggers Packagist's 5-minute polling.

## Verification

After registration, verify the package:

1. **Packagist page**: https://packagist.org/packages/satwareag/php-firebird-stubs
2. **Composer install test**:
   ```bash
   composer require --dev satwareag/php-firebird-stubs:dev-main
   ```

## Maintenance

### Token Rotation (Every 90 Days)

1. Create new fine-grained PAT (Step 1)
2. Update GitHub secret `STUBS_REPO_TOKEN`
3. Delete old PAT

### Version Tagging

To release a new version:
```bash
# In the main php-firebird repository
git tag v7.0.0
git push origin v7.0.0
```

The split workflow will propagate the tag to `php-firebird-stubs`, and Packagist will automatically pick up the new version.

## Troubleshooting

### "Package already exists" error
The package is already registered. Use the update endpoint instead:
```bash
curl -X POST \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer YOUR_USERNAME:YOUR_MAIN_API_TOKEN' \
  'https://packagist.org/api/update-package' \
  -d '{"repository":"https://github.com/satwareAG/php-firebird-stubs"}'
```

### Split workflow fails with "Permission denied"
- Verify `STUBS_REPO_TOKEN` secret exists
- Verify PAT has "Contents: Read and write" permission
- Verify PAT has access to `satwareAG/php-firebird-stubs`

### Split workflow fails with "Public access token is missing"
The PAT must be passed as an environment variable `PAT`, not as an input:
```yaml
- uses: danharrin/monorepo-split-github-action@v2.4.0
  env:
    PAT: ${{ secrets.STUBS_REPO_TOKEN }}
  with:
    # ... other inputs
```

### Split workflow fails with "Author identity unknown"
Add git identity configuration to the workflow:
```yaml
with:
  user_email: 'github-actions[bot]@users.noreply.github.com'
  user_name: 'github-actions[bot]'
```

### Packagist shows outdated version
Trigger a manual update:
```bash
curl -X POST \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer YOUR_USERNAME:YOUR_MAIN_API_TOKEN' \
  'https://packagist.org/api/update-package' \
  -d '{"repository":"https://github.com/satwareAG/php-firebird-stubs"}'
```

---

**Created**: 2026-01-03
**Related**: 
- [Mono-repo Stubs Strategy](../research/mono-repo-stubs-strategy.md)
- [GitHub Workflow](.github/workflows/split-stubs.yml)
