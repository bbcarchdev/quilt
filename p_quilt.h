#ifndef P_QUILT_H_
# define P_QUILT_H_                     1

# include <stdlib.h>
# include <string.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <fcgiapp.h>

# include "libsupport.h"
# include "libquilt-internal.h"

/* FastCGI run-loop */
int fcgi_init(void);
int fcgi_runloop(void);

#endif /*!P_QUILT_H_ */
