/* coref: This engine processes requests for coreference graphs populated
 * by Twine's "spindle" post-processing module.
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

#ifndef P_COREF_H_
# define P_COREF_H_                     1

# include <stdlib.h>
# include <string.h>
# include <libsparqlclient.h>

# include "libquilt.h"

# define QUILT_PLUGIN_NAME              "coref"

#endif /*!P_COREF_H_*/