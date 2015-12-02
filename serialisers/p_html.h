#ifndef P_HTML_H_
# define P_HTML_H_                     1

# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <librdf.h>

# include "libquilt.h"
# include "libliquify.h"

# define QUILT_PLUGIN_NAME              "html"

# define NS_RDF                         "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
# define NS_RDFS                        "http://www.w3.org/2000/01/rdf-schema#"
# define NS_DCT                         "http://purl.org/dc/terms/"
# define NS_GEO                         "http://www.w3.org/2003/01/geo/wgs84_pos#"
# define NS_OLO                         "http://purl.org/ontology/olo/core#"
# define NS_FOAF                        "http://xmlns.com/foaf/0.1/"

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
extern char *html_baseuri;
extern size_t html_baseurilen;

int html_template_init(void);
LIQUIFYTPL *html_template(QUILTREQ *req);

/* Check whether a MIME type is handled by this module */
int html_type(const char *type);
/* Attempt to determine which of the known classes the subject belongs to */
struct class_struct *html_class_match(librdf_model *model, librdf_node *subject);

int html_add_common(json_t *dict, QUILTREQ *req);
int html_add_request(json_t *dict, QUILTREQ *req);
int html_add_model(json_t *dict, QUILTREQ *req);

/* Debian Wheezy ships with libjansson 2.3, which doesn't include
 * json_array_foreach()
 */
# ifndef json_array_foreach
#  define json_array_foreach(array, index, value) \
	for(index = 0; index < json_array_size(array) && (value = json_array_get(array, index)); index++)
# endif

#endif /*!P_HTML_H_*/
