/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_quilt.h"

int
quilt_config_defaults(void)
{
	config_set_default("global:configFile", SYSCONFDIR "/quilt.conf");
	config_set_default("log:level", "notice");
	config_set_default("log:facility", "daemon");
	config_set_default("log:syslog", "1");
	config_set_default("log:stderr", "0");
	config_set_default("sparql:query", "http://localhost/sparql/");
	config_set_default("fastcgi:socket", "/tmp/quilt.sock");	
	config_set_default("quilt:base", "http://www.example.com/");
	return 0;
}
