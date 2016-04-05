FROM debian:wheezy
MAINTAINER Simeon van der Steen <simeon.vandersteen@bbc.co.uk>

# Add bbcarchdev apt repo
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 9E16A4D302DA096A && \
	echo "deb [arch=amd64] http://apt.bbcarchdev.net/debian/ wheezy main ports live stage dev" \
	>> /etc/apt/sources.list.d/bbcarchdev-wheezy.list
	
# Install everything needed, then clean
RUN apt-get -y update && apt-get install -y --no-install-recommends \
	quilt-templates-res quilt-fcgi quilt-common quilt-engine-spindle \
	apache2 libapache2-mod-fcgid \
	&& rm -rf /var/lib/apt/lists/*
	
# Copy the config file
COPY quilt.conf /usr/etc/quilt.conf
RUN ln -s /usr/etc/quilt.conf /etc/quilt.conf

# Link the FCGI script
RUN ln -s /usr/sbin/quilt-fcgid /usr/share/quilt-templates-res/public/index.fcgi

# Configure apache
ENV APACHE_RUN_USER	 www-data
ENV APACHE_RUN_GROUP www-data
ENV APACHE_LOG_DIR	/var/log/apache2
ENV APACHE_PID_FILE	/var/run/apache2.pid
ENV APACHE_RUN_DIR	/var/run/apache2
ENV APACHE_LOCK_DIR	/var/lock/apache2
RUN a2enmod rewrite
RUN mkdir /var/run/apache2
COPY apache2.conf /etc/apache2/sites-available/quilt
RUN a2dissite 000-default && a2ensite quilt
RUN	ln -sf /dev/stdout /var/log/apache2/quilt.access.log && \
	ln -sf /dev/stderr /var/log/apache2/error.log
EXPOSE 80

# Copy the run script for the configuration
COPY run.sh /
ENTRYPOINT [ "/run.sh" ]

# Launch apache2
CMD [ "apache2", "-DFOREGROUND" ]
