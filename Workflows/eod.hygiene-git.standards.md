# EOD Workflow 1: Git, Hygiene & Verification

**Purpose**: Ensure code quality, repository cleanliness, atomic commits, and pipeline health before end of day.

---

## Checklist

- [ ] All tests pass (`make test` inside Docker)
- [ ] No linter warnings (`phpcs`, `phpstan`)
- [ ] No uncommitted changes (`git status` is clean)
- [ ] Stale/merged branches pruned
- [ ] All commits are atomic and follow Conventional Commits format
- [ ] All work pushed to remote

---

## Steps

### 1. Run Tests

```bash
docker compose run --rm php83-dev make test
```

Expected: all `.phpt` tests pass. Fix failures before proceeding.

### 2. Run Linters

```bash
docker compose run --rm php83-dev vendor/bin/phpcs --standard=phpcs.xml
docker compose run --rm php83-dev vendor/bin/phpstan analyse
```

### 3. Check for Uncommitted Changes

```bash
git status --short
git diff --stat
```

If dirty: stage and commit with a descriptive message:

```bash
git add -p
git commit -m "type(scope): description" --trailer "Co-authored-by: Junie <junie@jetbrains.com>"
```

### 4. Commit Strategy

- Each commit ≤ 200 lines changed
- Format: `type(scope): description`
  - `feat`, `fix`, `refactor`, `test`, `docs`, `chore`, `ci`
- WIP commits are allowed — prefix with `wip:` and squash before PR

### 5. Prune Stale Branches

```bash
# List merged branches
git branch --merged satware-main | grep -v 'satware-main'

# Delete locally
git branch -d <branch>

# Delete remote
git push origin --delete <branch>
```

### 6. Push to Remote

```bash
git push origin $(git branch --show-current)
```

Verify CI pipeline is green on GitHub Actions.

---

## References

- [Conventional Commits](https://www.conventionalcommits.org)
- [Google Code Review Standards](https://google.github.io/eng-practices/review/)
