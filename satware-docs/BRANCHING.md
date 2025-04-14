# Branching Strategy

Our fork maintains several important branches:

## Core Branches

- **upstream-mirror** - Direct mirror of upstream's master branch, never modified directly
- **satware-main** - Our primary development branch, all features target this branch
- **satware-stable** - Stabilized code ready for release
- **release/v{X.Y.z}** - Release branches for each major/minor version

## Feature Branches

All development should occur on feature branches:

- Name format: `feature/{short-description}`
- Create from: `satware-main`
- Merge to: `satware-main`

## Workflow Diagram

```
upstream/master
^
|
upstream-mirror
|
+---------> satware-main ---------> release/v1.0.x
|                         |
|                         +-> v1.0.0
|                         |
|                         +-> v1.0.1
|
+-> feature/x
|
+-> fb25-compat
|
+-> fb30-compat
```

## Syncing with Upstream

We selectively pull changes from upstream:

1. Update our upstream-mirror branch with upstream/master
2. Cherry-pick specific fixes or enhancements to satware-main
3. Document all incorporated upstream changes

See the [upstream sync workflow](workflows/sync.md) for detailed procedures.



```

## 3. Update Main README.md

Modify the repository's main README.md to include a section about your fork:

```markdown
# PHP Firebird Extension (satwareAG Fork)

This is satwareAG's enhanced fork of the PHP Firebird extension, focusing on stability and performance for applications using Firebird 2.5 through 5.0.

## Fork Enhancements

- Improved transaction handling
- Enhanced memory management
- Expanded error reporting
- Comprehensive multi-version testing

## Documentation

- For contributing to this fork, see our [satware-docs/](satware-docs/) directory
- For discussions and questions, visit our [Discussions](https://github.com/satwareAG/php-firebird/discussions) section

## Original Documentation

[Preserve original documentation here]
```

## 4. Set Up GitHub Discussions

1. Go to your repository on GitHub
2. Click on "Settings"
3. Scroll down to "Features" section
4. Check "Discussions" to enable
5. Click "Set up discussions"

Create these initial discussion categories:

- **Announcements**: Important updates about the fork
- **Development Environment**: Questions about setup across platforms
- **Workflow Help**: Questions about the development process
- **Ideas**: Suggestions for improvements

Create an initial welcome post:

```markdown
# Welcome to the satwareAG PHP Firebird Extension Discussions

This is a space to discuss development of our enhanced fork of the PHP Firebird extension. 

## Resources

- See our [documentation](https://github.com/satwareAG/php-firebird/tree/satware-main/satware-docs) for getting started
- Check our [issues](https://github.com/satwareAG/php-firebird/issues) for current work
- View our [project board](https://github.com/satwareAG/php-firebird/projects) for development status

## Guidelines

- Keep discussions respectful and on-topic
- Use appropriate categories for your posts
- Search before creating a new discussion
```

## 5. Create a Project Board

1. Go to "Projects" tab in your repository
2. Click "Create project"
3. Select "Board" view
4. Create columns: "To Do", "In Progress", "Under Review", "Done"

Add these initial items to track documentation:

- "Complete environment guides" - To Do
- "Set up CI/CD for testing" - To Do
- "Document upstream sync process" - In Progress

## Implementation Timeline

1. **Day 1**:
    - Set up directory structure
    - Create core documentation files

2. **Day 2-3**:
    - Complete environment guides for IntelliJ
    - Set up GitHub Discussions categories

3. **Day 4-5**:
    - Complete workflow documentation
    - Update main README.md
    - Create Project Board

4. **Ongoing**:
    - Regularly update docs as workflows evolve
    - Expand environment guides based on team needs

This approach provides clear documentation that works well with your forked repository structure, avoids conflicts with upstream, and leverages GitHub features appropriate for forks.