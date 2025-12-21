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

Default credentials:
- Username: `SYSDBA`
- Password: `masterkey`
- Database: `test.fdb`

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
