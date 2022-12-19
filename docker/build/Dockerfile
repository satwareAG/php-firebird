FROM php:8.1-bullseye

RUN apt-get update && apt-get install -y git
RUN apt-get install -y firebird-dev
RUN DEBIAN_FRONTEND=noninteractive /usr/bin/apt-get install -y -q --force-yes firebird3.0-server
RUN docker-php-source extract \
    && mkdir -p /usr/src/php/ext/interbase \
    && mkdir -p /usr/src/build-module
COPY . /usr/src/php/ext/interbase
RUN cd /usr/src/php/ext/interbase && phpize && CPPFLAGS=-I/usr/include/firebird ./configure && make && echo n | make test
