# Setting Up IntelliJ IDEA with GitToolbox

This guide will help you configure IntelliJ IDEA Ultimate with GitToolbox for developing the PHP Firebird extension.

## Prerequisites

- IntelliJ IDEA Ultimate 2023.3 or newer
- GitToolbox plugin (600.1.1+) installed
- Git configured with SSH keys for GitHub

## Initial Repository Setup

1. **Clone the Repository**
    - Open IntelliJ IDEA
    - Select **Get from VCS** on the welcome screen
    - Under **Version Control**, select **GitHub**
    - Choose `satwareAG/php-firebird` from the list
    - Set the local directory and click **Clone**

2. **Configure Remote Repositories**
    - Go to **Git → Manage Remotes**
    - Add upstream: `https://github.com/FirebirdSQL/php-firebird.git`

3. **Set Up GitToolbox**
    - Go to **Settings/Preferences → Tools → GitToolbox**
    - Enable Auto Fetch (every 15 minutes recommended)
    - Enable Status Presenter
    - Configure Inline Blame as desired

## Working with Branches

### Updating from Upstream
1. Switch to `upstream-mirror` branch
2. Pull from upstream remote
3. Push to origin to update your fork
4. See the [upstream sync workflow](../workflows/sync.md)

### Creating Feature Branches
1. Ensure you're on `satware-main` branch
2. Use **Git → New Branch** and name it `feature/description`
3. Make your changes, commit, and push

## GitToolbox Features

- Use the status bar widget to monitor branch status
- Access quick branch switching with **Ctrl+Shift+`**
- View local changes status in the editor

## PHP Development Setup

1. Configure PHP Interpreter
    - Go to **Settings/Preferences → PHP**
    - Add PHP 8.1+ interpreter
    - Ensure Firebird extension is enabled

2. Configure Build Settings
    - [PHP Extension build settings details]
