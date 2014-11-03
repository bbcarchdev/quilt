#ifndef P_HTML_H_
# define P_HTML_H_                      1

# define _BSD_SOURCE                    1

# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <librdf.h>

# include "libquilt.h"
# include "libliquify.h"

# define QUILT_PLUGIN_NAME              "html"

struct class_struct
{
	const char *uri;
	const char *cssClass;
	const char *label;
	const char *suffix;
	const char *definite;
};

extern QUILTTYPE html_types[];
extern struct class_struct html_classes[];

/* Check whether a MIME type is handled by this module */
int html_type(const char *type);
/* Attempt to determine which of the known classes the subject belongs to */
struct class_struct *html_class_match(librdf_model *model, librdf_node *subject);

#endif /*!P_HTML_H_*/
