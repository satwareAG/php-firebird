# Contributing to PHP Firebird Extension

Thank you for your interest in contributing to the modernized PHP Firebird extension. This document outlines our development standards and processes for PHP 8.2+ and C++17 modernization.

## Development Standards (2025)

### Target Specifications
- **PHP Version**: 8.2+ only (PHP 7 support dropped)
- **C++ Standard**: C++17 (upgraded from C++11)
- **Firebird Versions**: 3.0, 4.0, 5.0+
- **Build System**: GNU Autotools with cross-platform support

### Development Philosophy
- **Memory Safety First**: Comprehensive static and dynamic analysis
- **Modern C++ Practices**: Leverage C++17 features for safety and performance
- **Cross-Platform**: Linux, Windows, macOS support with consistent tooling
- **Production Ready**: Enterprise-grade code quality and testing

## Development Environment Setup

### Docker Development (Recommended)
```bash
# Use our Docker development environment
cd docker/
docker-compose up -d php81  # or php82, php83, php84, php85

# Build extension in container
docker exec -it php-firebird-dev /ext/scripts/container/build.sh
```

### Native Development Requirements
- **Compilers**: GCC 7+ or Clang 5+ (C++17 support)
- **PHP**: 8.1+ with php-dev/php-devel packages
- **Firebird**: Client libraries and headers (fbclient, ibase.h)
- **Build Tools**: autotools, make, pkg-config

### IDE Configuration
- **EditorConfig**: Automatically configured via `.editorconfig`
- **Code Style**: 4-space indentation, 100-char line limit for C/C++, 120 for PHP
- **Line Endings**: LF (Unix-style) for all files except Windows batch files

## Code Quality Standards

### Static Analysis Tools (MANDATORY)
All code must pass these checks before submission:

```bash
# C++ Static Analysis
clang-tidy src/*.cpp src/*.h --checks='*,-fuchsia-*'
cppcheck --enable=all --std=c++17 src/

# Memory Safety (Development)
# Compile with sanitizers for testing
CXXFLAGS="-fsanitize=address,undefined" ./configure
make && make test
```

### Code Standards
- **C++17 Features**: Use modern constructs (auto, range-for, smart pointers)
- **Memory Management**: RAII principles, avoid raw pointers
- **Error Handling**: Exception safety, proper resource cleanup
- **Naming**: snake_case for variables/functions, PascalCase for classes
- **Documentation**: Doxygen comments for public interfaces

### Testing Requirements
- **Test Coverage**: All new functionality must include tests
- **Test Framework**: PHP .phpt test format
- **Cross-Platform**: Tests must pass on Linux, Windows, macOS
- **Memory Testing**: Valgrind clean on Linux builds

## Build System Modernization

### Configuration Updates
The `config.m4` has been updated to:
- Drop PHP 7 compatibility checks
- Target C++17 standard: `PHP_CXX_COMPILE_STDCXX([17], [mandatory])`
- Enhanced Firebird version detection

### Build Commands
```bash
# Standard build process
phpize
./configure --with-firebird
make

# Development build with debugging
CXXFLAGS="-g -O0 -fsanitize=address" ./configure --with-firebird
make

# Clean build environment
make clean && phpize --clean
```

## Development Workflow

### 1. Setup and Branching
```bash
git clone https://github.com/satwareAG/php-firebird.git
cd php-firebird
git checkout -b feature/your-feature-name
```

### 2. Code Development
- Follow .editorconfig standards automatically
- Write tests first (TDD approach recommended)
- Use static analysis tools during development
- Test with multiple PHP and Firebird versions

### 3. Pre-commit Checklist
- [ ] Code compiles without warnings on GCC and Clang
- [ ] All tests pass: `make test`
- [ ] Static analysis clean: `clang-tidy` and `cppcheck`
- [ ] Memory safety verified: AddressSanitizer clean
- [ ] Cross-platform testing completed
- [ ] Documentation updated for API changes

### 4. Pull Request Process
1. **Title**: Use conventional commits format (feat:, fix:, refactor:)
2. **Description**: Include motivation, changes, testing details
3. **Testing**: Provide evidence that changes work across environments
4. **Review**: Address all feedback before merge

## Modern Development Tools Integration

### Static Analysis Configuration
Create these configuration files for consistent analysis:

**.clang-tidy** (project root):
```yaml
Checks: >
  *,
  -fuchsia-*,
  -google-runtime-references,
  -readability-identifier-length
```

### IDE Integration
- **CLion**: Native CMake support, integrated debugger
- **VS Code**: C/C++ extension with clang-tidy integration
- **Visual Studio**: Native project support for Windows builds

### Debugging Tools
- **GDB/LLDB**: Standard debugging with PHP symbols
- **Valgrind**: Memory error detection (Linux)
- **AddressSanitizer**: Fast memory error detection (all platforms)
- **XDebug**: PHP-level debugging integration

### Memory Debugging: ASan vs Valgrind

We use **both** AddressSanitizer and Valgrind as complementary tools, following PHP core team practices.

**Tool Comparison:**

| Aspect | AddressSanitizer | Valgrind |
|--------|------------------|----------|
| **Speed** | 2-4x slowdown | 20-50x slowdown |
| **Compile-time** | Required | Not required |
| **Heap buffer overflow** |  |  |
| **Stack buffer overflow** |  |  |
| **Use-after-free** |  |  |
| **Uninitialized reads** |  |  |
| **Memory leaks** | Basic | Comprehensive |

**When to Use Each:**

| Scenario | Recommended Tool |
|----------|------------------|
| Daily development | ASan |
| CI fast feedback | ASan |
| Fuzzing | ASan |
| Pre-release QA | Both |
| Investigating specific leaks | Valgrind |
| Uninitialized memory bugs | Valgrind |

**Running ASan (Primary Tool):**
```bash
# Use the ASan-enabled PHP container
docker-compose -f docker/docker-compose.yml run php83-asan

# Or compile with ASan flags
CXXFLAGS="-fsanitize=address,undefined -g" ./configure --with-firebird
make && make test

# Run fuzzer with ASan
./scripts/fuzz_asan.sh
```

**Running Valgrind (Complementary Tool):**
```bash
# Quick check (essential tests only)
./scripts/run-valgrind.sh --quick

# Full test suite
./scripts/run-valgrind.sh

# Specific test
./scripts/run-valgrind.sh tests/fbird_blob_001.phpt

# Save output to log file
./scripts/run-valgrind.sh --log valgrind-report.txt
```

**Critical Environment Variables for Valgrind:**
- `USE_ZEND_ALLOC=0`: Bypass Zend Memory Manager (required for accurate tracking)
- `ZEND_DONT_UNLOAD_MODULES=1`: Keep modules loaded for accurate stack traces

The `scripts/run-valgrind.sh` script sets these automatically.

**Suppressions:**
Known false positives are suppressed via `valgrind-php.supp`. Add new suppressions only after confirming they are genuine false positives.

**Reference:** See `docs/research/asan-vs-valgrind-php-extensions.md` for detailed research and rationale.

## Architecture Considerations

### PHP Extension Constraints
- **Zend API Compliance**: All code must work within Zend memory management
- **Thread Safety**: Consider ZTS vs NTS implications
- **Resource Management**: Proper cleanup in error conditions
- **PHP Object Lifecycle**: Understanding of PHP's garbage collection

### Firebird Integration
- **API Version Support**: Handle multiple fbclient API versions
- **Character Sets**: Proper UTF-8 and multi-byte handling
- **Transaction Management**: ACID compliance and rollback safety
- **Connection Pooling**: Efficient resource utilization

### Firebird OO API Development (C++ Wrappers)

The extension uses modern C++ OO API wrappers for Firebird 3.0+ with C interop functions. Follow these conventions:

**C Interop Function Families:**
| Family | Prefix | Purpose | Files |
|--------|--------|---------|-------|
| Connection | `fbc_*` | `IAttachment` lifecycle | `fb_connection.hpp` |
| Transaction | `fbt_*` | `ITransaction` lifecycle | `fb_transaction.hpp` |
| Statement | `fbs_*` | `IStatement` + `IResultSet` cursor | `fb_statement.hpp` |

**RAII Wrapper Pattern:**
```cpp
// All wrappers follow this pattern in src/cpp/
class ConnectionWrapper {
    Firebird::IAttachment* attachment_ = nullptr;
    bool owns_attachment_ = false;
public:
    bool connect(Firebird::IMaster* master, ...);
    void disconnect(ISC_STATUS* status_vector);
    // Destructor cleans up automatically
};
```

**C Interop Function Conventions:**
```c
// Extern "C" linkage for C code access
void* fbc_connect(void* master, const char* database, ...);
int fbc_disconnect(void* connection, ISC_STATUS* status_vector);
int fbc_is_connected(void* connection);
```

**Struct Field Additions:**
When adding OO API pointers to structs, use `#if FB_API_VER >= 30`:
```c
typedef struct {
    isc_db_handle handle;           // Legacy handle
#if FB_API_VER >= 30
    void *fbc_connection;           // OO API wrapper pointer
#endif
} fbird_db_link;
```

**Critical: Initialize OO API Pointers:**
Always initialize OO API pointer fields to `NULL` in ALL allocation paths:
```c
trans->fbt_transaction = NULL;  // MANDATORY to prevent segfaults
```

**FB 4.0 Compatibility:**
Use `statusHasError()` helper instead of FB 5.0-only `IStatus::hasData()`:
```cpp
// src/cpp/fb_status.hpp
bool statusHasError(Firebird::IStatus* status) {
    return (status->getState() & Firebird::IStatus::STATE_ERRORS) != 0;
}
```

See [MODERNIZATION_PLAN_FB3_TO_FB5.md](docs/development/MODERNIZATION_PLAN_FB3_TO_FB5.md) for complete implementation details.

## Testing Strategy

### Test Categories
1. **Unit Tests**: Individual function testing (.phpt files)
2. **Integration Tests**: Full database interaction scenarios  
3. **Cross-Version Tests**: Multiple PHP and Firebird versions
4. **Performance Tests**: Benchmarking critical operations
5. **Memory Tests**: Leak detection and bounds checking

### Testing Environments
Use our Docker setup or test against:
- PHP 8.2, 8.3, 8.4, 8.5-dev
- Firebird 3.0, 4.0, 5.0+
- Linux (Ubuntu, CentOS), Windows 10/11, macOS

## Performance Considerations

### Optimization Guidelines
- **Connection Reuse**: Minimize connection overhead
- **Prepared Statements**: Efficient query execution
- **Buffer Management**: Optimal memory usage patterns
- **Error Path Performance**: Fast failure handling

### Profiling Tools
- **Xdebug Profiler**: PHP-level performance analysis
- **perf/gprof**: C++ level profiling
- **Valgrind Callgrind**: Detailed call analysis

## Documentation Requirements

### Code Documentation
- **Header Comments**: File purpose, author, modification history
- **Function Documentation**: Parameters, return values, exceptions
- **API Changes**: Document breaking changes in CHANGELOG.md

### User Documentation
- **README Updates**: Installation and usage instructions
- **Examples**: Working code samples for new features
- **Migration Guides**: Breaking change migration paths

## Release Process

### Version Numbering
Follow semantic versioning: MAJOR.MINOR.PATCH
- **MAJOR**: Breaking changes (PHP version drops, API changes)
- **MINOR**: New features, Firebird version support
- **PATCH**: Bug fixes, performance improvements

### Release Checklist
- [ ] All tests pass across supported environments
- [ ] Documentation updated (README, CHANGELOG)
- [ ] Performance regression testing completed
- [ ] Security scanning completed
- [ ] Cross-platform build verification

## Getting Help

- **Issues**: Report bugs and feature requests on GitHub
- **Discussions**: Use GitHub Discussions for questions
- **Security**: Email security issues privately to maintainers
- **Documentation**: Check our docs/ directory for detailed guides

---

*This document reflects the 2025 modernization standards. For legacy information, see archived documentation.*