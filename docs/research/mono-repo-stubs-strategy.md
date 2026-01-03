# Mono-Repo Strategy for php-firebird-stubs

## Decision Summary

**Chosen Approach**: Mono-repo with subtree split capability

The stubs package (`satwareag/php-firebird-stubs`) lives in the `stubs/` subdirectory of the main `php-firebird` repository. This is the recommended approach for PHP extension stubs based on industry best practices.

## Why Mono-Repo?

### Advantages for Development

1. **Single Source of Truth**: Stubs stay synchronized with extension code
2. **Atomic Commits**: Update extension and stubs in one commit
3. **Easier Maintenance**: No cross-repo PRs needed
4. **Version Alignment**: Stubs version matches extension version naturally
5. **AI-Friendly**: Single workspace for both extension and stubs development

### Industry Precedents

- **Laravel**: Packages developed in mono-repo, split for distribution
- **Symfony**: Components in mono-repo with read-only split repos
- **TYPO3**: Core and extensions managed together with automated splits
- **SocialiteProviders**: Uses subtree splits for individual packages

## Publishing Options

### Option 1: Packagist Path Repository (Development/Testing)

For local development, users can reference the stubs directly:

```json
{
    "repositories": [
        {
            "type": "path",
            "url": "./stubs"
        }
    ],
    "require-dev": {
        "satwareag/php-firebird-stubs": "@dev"
    }
}
```

### Option 2: Git Subtree Split (Production)

For Packagist distribution, use `git subtree split` to create a read-only repository:

```bash
# Create subtree split
git subtree split --prefix=stubs -b stubs-release

# Push to dedicated repository
git push git@github.com:satwareAG/php-firebird-stubs.git stubs-release:main

# Tag release (in split repo)
cd /path/to/php-firebird-stubs
git tag v7.0.0
git push origin v7.0.0
```

### Option 3: Automated Subtree Split (CI/CD)

Using GitHub Actions for automated splits on release:

```yaml
# .github/workflows/split-stubs.yml
name: Split Stubs Package

on:
  push:
    tags:
      - 'v*'

jobs:
  split:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      
      - name: Split stubs subdirectory
        run: |
          git subtree split --prefix=stubs -b stubs-release
      
      - name: Push to stubs repository
        run: |
          git push https://x-access-token:${{ secrets.STUBS_REPO_TOKEN }}@github.com/satwareAG/php-firebird-stubs.git stubs-release:main --force
          git push https://x-access-token:${{ secrets.STUBS_REPO_TOKEN }}@github.com/satwareAG/php-firebird-stubs.git ${{ github.ref }}
```

## Packagist Registration

### Manual Steps (One-Time Setup)

1. Create separate repository `satwareAG/php-firebird-stubs` (if using subtree split)
2. Perform initial subtree split and push
3. Register on Packagist: https://packagist.org/packages/submit
4. Enter repository URL: `https://github.com/satwareAG/php-firebird-stubs`
5. Configure webhook for auto-updates

### Alternative: VCS Repository (Without Split)

Users can install directly from the mono-repo stubs directory:

```json
{
    "repositories": [
        {
            "type": "vcs",
            "url": "https://github.com/satwareAG/php-firebird.git"
        }
    ],
    "require-dev": {
        "satwareag/php-firebird-stubs": "dev-feature/dev-7.0.0"
    }
}
```

This requires Composer 2.x and works but is less clean than a proper Packagist package.

## Recommended Workflow

### For This Project

1. **Development**: Work in mono-repo `stubs/` directory
2. **Release**: Tag extension release (e.g., v7.0.0)
3. **Split**: Run subtree split to dedicated stubs repo
4. **Publish**: Tag same version in stubs repo
5. **Packagist**: Auto-updates via webhook

### Version Synchronization

| Extension Version | Stubs Version | Notes |
|-------------------|---------------|-------|
| v7.0.0            | v7.0.0        | Initial release |
| v7.0.1            | v7.0.1        | Patch (if stubs changed) |
| v7.1.0            | v7.1.0        | Minor release |

Stubs version should always match extension version to avoid confusion.

## Tools for Mono-Repo Management

### Recommended: symplify/monorepo-builder

```bash
composer require --dev symplify/monorepo-builder

# Generate packages list
vendor/bin/monorepo-builder packages-json

# Merge composer configs
vendor/bin/monorepo-builder merge

# Split packages
vendor/bin/monorepo-builder split
```

### Alternative: splitsh/lite

Faster for large repositories, C implementation:

```bash
# Install
brew install splitsh/lite/splitsh-lite  # macOS
# or download binary from GitHub releases

# Split
splitsh-lite --prefix=stubs --target=refs/heads/stubs-release
```

## Current Implementation Status

- ✅ `stubs/` directory created in mono-repo
- ✅ `stubs/composer.json` with package metadata
- ✅ `stubs/firebird-stubs.php` with functions and constants
- ✅ `stubs/firebird-classes.php` with extension classes
- ✅ `stubs/README.md` with installation instructions
- ✅ `stubs/LICENSE` (PHP License 3.01)
- ✅ Main README.md updated with stubs documentation
- ⏳ Packagist registration (pending subtree split or mono-repo decision)
- ⏳ CI/CD automation for splits (optional)

## References

- [Composer Path Repositories](https://getcomposer.org/doc/05-repositories.md#path)
- [Git Subtree Manual](https://git-scm.com/book/en/v2/Git-Tools-Subtree-Merging)
- [symplify/monorepo-builder](https://github.com/symplify/monorepo-builder)
- [splitsh/lite](https://github.com/splitsh/lite)
