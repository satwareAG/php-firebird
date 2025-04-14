# Contributing to the satwareAG PHP Firebird Extension

Thank you for your interest in contributing to our enhanced fork of the PHP Firebird extension. This document outlines the process for contributing to this project.

## Development Philosophy

Our fork focuses on:
- Stability for production environments using Firebird 2.5 and 3.0
- Compatibility with newer Firebird versions (4.0, 5.0)
- Optimized performance for Doctrine DBAL integration
- Comprehensive testing across all supported environments

## Getting Started

1. **Set up your development environment**
    - See our [environment guides](environments/) for IDE-specific instructions
    - Ensure you have appropriate Firebird client libraries installed

2. **Understand our branching strategy**
    - Review our [branching strategy](BRANCHING.md) before creating any branches
    - All development happens against the `satware-main` branch

3. **Find an issue to work on**
    - Check our [GitHub Issues](https://github.com/satwareAG/php-firebird/issues)
    - Issues labeled `good-first-issue` are ideal for new contributors

## Development Workflow

1. Create a feature branch from `satware-main`
2. Implement your changes with appropriate tests
3. Submit a pull request
4. Respond to code review feedback

See our [workflows directory](workflows/) for detailed procedures.

## Code Style and Standards

- Follow the existing code style in the codebase
- All new code must include appropriate tests
- Document any API changes or new features
