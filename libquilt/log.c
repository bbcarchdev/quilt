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

#include "p_libquilt.h"

quilt_log_fn quilt_logger_;

int
quilt_log_init_(quilt_log_fn logfn)
{
	quilt_logger_ = logfn;
	return 0;
}

void
quilt_vlogf(int prio, const char *format, va_list args)
{
	if(quilt_logger_)
	{
		quilt_logger_(prio, format, args);
	}
}

void
quilt_logf(int prio, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	quilt_vlogf(prio, format, ap);
}
