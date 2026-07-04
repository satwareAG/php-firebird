# Docker Development Environment for PHP Firebird Extension

This document describes how to use the Docker-based development environment for the PHP Firebird extension.

## Prerequisites

- Docker Engine 26.0 or later
- Docker Compose V2
- IntelliJ IDEA Ultimate / CLion with Docker plugin enabled

## Getting Started

1. **Build the Docker images**:
   ```bash
   cd docker
   docker compose build
   ```

2. **Start the environment**:
   ```bash
   docker compose up -d
   ```

3. **Build the extension**:
   ```bash
   docker compose exec php83-dev sh -c "cd /ext && phpize --clean && phpize && ./configure && make clean && make -j\$(nproc)"
   ```

## Using CLion / IntelliJ IDEA Run Configurations

Several run configurations are provided for convenience:

- **Build PHP x.y**: Builds the extension with the specified PHP version
- **Test PHP x.y**: Runs tests using the specified PHP version
- **PHP x.y Shell**: Opens a shell in the specified PHP container
- **Connect Firebird x.y DB**: Tests a connection to the specified Firebird database

These configurations can be found in the Run menu or using the configuration selector in the toolbar.

## Firebird Database Access

The development environment includes Firebird database servers on these ports:

- Firebird 2.5: `localhost:3050`
- Firebird 3.0: `localhost:3051`
- Firebird 4.0: `localhost:3052`
- Firebird 5.0: `localhost:3053`

Default credentials:
- Username: `SYSDBA`
- Password: `masterkey`
- Database: `test.fdb`

## Firebird Client Version Testing

The development environment supports testing with different Firebird client library versions.

### Standard Containers (Firebird 4.x Client)

By default, PHP containers use the apt-installed Firebird 4.x client library:
- `php81-dev`, `php82-dev`, `php83-dev`, `php84-dev`, `php85-dev`

These containers can connect to all Firebird server versions (2.5, 3.0, 4.0, 5.0).

### Firebird 3.0 Client Container

For testing compilation against Firebird 3.0 client headers (Issue #19 compatibility):
```bash
docker compose exec php84-fb3-dev sh -c "cd /ext && phpize --clean && phpize && ./configure --with-firebird=/opt/firebird && make -j\$(nproc)"
```

The `php84-fb3-dev` container:
- Uses FB 3.0.12 client library
- Compatible with FB 2.5 and 3.0 servers
- Firebird installed in `/opt/firebird`
- Environment: `FIREBIRD_HOME=/opt/firebird`

### Firebird 5.0 Client Container

For testing with the latest Firebird 5.x client:
```bash
docker compose exec php85-fb5-dev sh -c "cd /ext && phpize --clean && phpize && ./configure --with-firebird=/opt/firebird && make -j\$(nproc)"
```

The `php85-fb5-dev` container:
- Uses FB 5.0.3 client library
- Best compatibility with FB 5.0 server features
- Firebird installed in `/opt/firebird`
- Environment: `FIREBIRD_HOME=/opt/firebird`

### Code Quality Checks with Different Clients

The static analysis scripts auto-detect the Firebird installation:
```bash
# Run cppcheck in FB 3.0 client container
docker compose exec php84-fb3-dev sh -c "/ext/scripts/analysis/cppcheck.sh"

# Run clang-tidy in FB 5.0 client container
docker compose exec php85-fb5-dev sh -c "/ext/scripts/analysis/clang_tidy.sh"

# Generate compile_commands.json (auto-detects Firebird path)
docker compose exec php84-fb3-dev sh -c "/ext/scripts/analysis/generate_compdb.sh"
```

## Customization

To customize the environment for your local setup, copy `docker-compose.override.yml.example` to `docker-compose.override.yml` and modify as needed.

## Troubleshooting

### Extension Build Issues

If you encounter build issues, try accessing the container directly:

```bash
docker compose run --rm php83-dev bash
cd /ext
```

Then run the build steps manually to see detailed error messages.

### Database Connection Issues

To verify database connectivity:

```bash
docker compose run --rm php83-dev bash
ping firebird30
```

### Cleaning Up

To remove all containers and volumes:

```bash
docker compose down -v
```
