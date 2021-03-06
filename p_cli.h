/* Quilt: Command-line query interface
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

#ifndef P_CLI_H_
# define P_CLI_H_                       1

# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <unistd.h>

# include "libkvset.h"
# include "libsupport.h"

# define QUILTIMPL_DATA_DEFINED         1

typedef struct
{
	int headers_sent;
	KVSET *kv;
} QUILTIMPLDATA;

# include "libquilt-sapi.h"

#endif /*!P_CLI_H_ */
