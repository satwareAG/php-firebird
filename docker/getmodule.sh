#!/bin/sh
docker-compose build
docker-compose up -d
docker cp build-firebird-interbase-extension:/usr/src/php/ext/interbase/modules/interbase.so ./../artefacts/
docker cp build-firebird-interbase-extension:/usr/src/php/ext/interbase/modules/interbase.la ./../artefacts/
docker-compose down
docker-compose rm -f