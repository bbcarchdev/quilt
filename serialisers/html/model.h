#include "p_html.h"
#include <jansson.h>
#include <libquilt.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

static json_t *html_model_items_(QUILTREQ *req, librdf_model *model);
static int html_model_subject_(QUILTREQ *req, json_t *item, librdf_model *model, librdf_node *subject, const char *uri);
static int html_model_predicate_(QUILTREQ *req, json_t *value, librdf_node *predicate, const char *uri);
static int html_model_object_(QUILTREQ *req, json_t *value, librdf_node *object);
static char *html_model_abstract_(QUILTREQ *req, librdf_model *model, json_t *items, json_t **item);
static char *html_model_primaryTopic_(QUILTREQ *req, librdf_model *model, json_t *items, const char *abstractUri, json_t **item);
static json_t *html_model_results_(QUILTREQ *req, json_t *items);
static int html_model_item_is_(json_t *item, const char *classuri);
static char *get_title(QUILTREQ *req, librdf_model *model, librdf_node *subject);
static char *get_shortdesc(QUILTREQ *req, librdf_model *model, librdf_node *subject);
static char *get_longdesc(QUILTREQ *req, librdf_model *model, librdf_node *subject);
static char *get_literal(QUILTREQ *req, librdf_model *model, librdf_node *subject, const char *predicate);
static int cmp_index(const void *a, const void *b);
