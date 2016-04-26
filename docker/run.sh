#!/bin/bash
set -e

# TODO: Set default env vars somewhere in case they don't come with "docker run"

# Wait for postgres, if we're using spindle
if [ "${ENGINE}" = "spindle" ]; then
        until nc -z $POSTGRES_PORT_5432_TCP_ADDR $POSTGRES_PORT_5432_TCP_PORT; do
            echo "$(date) - waiting for postgres..."
            sleep 2
        done
fi

if [ ! -f /init-done ]; then
	echo "Initialising Quilt.."

	# TODO: Perhaps this one could be dynamically changed through Apache environment vars
	sed -i -e "s|HOST_NAME|${HOST_NAME-http://acropolis.org.uk}|" /etc/apache2/sites-available/quilt
	sed -i -e "s|HOST_NAME|http://${HOST_NAME-acropolis.org.uk}/|" /usr/etc/quilt.conf
	sed -i -e "s|ENGINE|${ENGINE-file}|g" /usr/etc/quilt.conf
	sed -i -e "s|DATA_DIR_TTL|${DATA_DIR_TTL-/data}|" /usr/etc/quilt.conf
	# Triple store settings
	sed -i -e "s|TRIPLESTORE_SPARQL|$TRIPLESTORE_SPARQL|" /usr/etc/quilt.conf
	# s3 settings
	sed -i -e "s|S3_PORT_4569_TCP_ADDR|$S3_PORT_4569_TCP_ADDR:$S3_PORT_4569_TCP_PORT|" /usr/etc/quilt.conf
	sed -i -e "s|S3_ENV_SPINDLE_BUCKET|$S3_ENV_SPINDLE_BUCKET|" /usr/etc/quilt.conf
	sed -i -e "s|S3_ENV_ACCESS_KEY|${S3_ENV_ACCESS_KEY-x}|" /usr/etc/quilt.conf
	sed -i -e "s|S3_ENV_SECRET_KEY|${S3_ENV_SECRET_KEY-x}|" /usr/etc/quilt.conf

	touch /init-done
fi

exec "$@"
