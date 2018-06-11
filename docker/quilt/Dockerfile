## You can use this as-is for a quick demonstration of Quilt using its default
## out-of-the-box configuration:
##
## $ docker run -p 4000:80 bbcarchdev/quilt
##
## (you can now point a browser at http://localhost:4000/)
##
## Alternatively, you can customise it by creating a Dockerfile containing
## something like:
##
## FROM bbcarchdev/quilt
## COPY my-quilt.conf /etc/quilt.conf
##

FROM bbcarchdev/bbcarchdev-base:live

RUN apt-get install -y quilt-cli quilt-fcgi apache2-mpm-prefork libapache2-mod-fcgid

RUN rm -f /etc/apache2/other/other-vhosts-access-log

# Configure Apache for Quilt
RUN a2enmod rewrite && \
	a2enmod fcgid && \
	a2dissite 000-default
	
COPY apache-quilt-fcgi.conf /etc/apache2/sites-available/apache-quilt-fcgi
RUN a2ensite apache-quilt-fcgi

# Cribbed from the official httpd Dockerfile
COPY httpd-foreground /usr/local/bin

EXPOSE 80
CMD ["httpd-foreground"]
