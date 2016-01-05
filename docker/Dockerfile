FROM acropolis-base
MAINTAINER Simeon van der Steen <simeon.vandersteen@bbc.co.uk>

ENV DEBIAN_FRONTEND noninteractive

ENV APACHE_RUN_USER	www-data
ENV APACHE_RUN_GROUP	www-data
ENV APACHE_LOG_DIR	/var/log/apache2
ENV APACHE_PID_FILE	/var/run/apache2.pid
ENV APACHE_RUN_DIR	/var/run/apache2
ENV APACHE_LOCK_DIR	/var/lock/apache2

# Install Quilt-specific dependencies
RUN apt-get install -y --no-install-recommends \
	liburi-dev libsparqlclient-dev \
	apache2 libapache2-mod-fcgid
RUN a2enmod rewrite

# Mount dir
COPY . /usr/local/src/
WORKDIR /usr/loca/src

# Compile
RUN 	autoreconf -i && \
	./configure --enable-debug && \
	make && \
	make install

# Adjust Apache config
RUN 	cp apache2-example.conf /etc/apache2/sites-available/quilt && \
	cd /etc/apache2/sites-available && \
	sed -i '/ServerName/d' quilt && \
	sed -i -e 's|@DATAROOTDIR@|/usr/local/share|' quilt

# Adjust Quilt config
RUN sed -i -e 's|; root=|root=|' /usr/local/etc/quilt.conf

RUN a2dissite 000-default && a2ensite quilt

EXPOSE 80

ENTRYPOINT [ "/usr/sbin/apache2", "-DFOREGROUND" ]

