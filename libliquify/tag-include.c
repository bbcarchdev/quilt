/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC.
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

#include "p_libliquify.h"

int
liquify_tag_include_parsed_(LIQUIFYTPL *template, struct liquify_part *part)
{
	struct liquify_expression *expr;

	if(!part->d.tag.pfirst)
	{
		PARTERRS(template, part, "expected: literal pathname (no parameters found)\n");
		return -1;
	}
	expr = &(part->d.tag.pfirst->expr);
	if(!EXPR_IS(expr, TOK_STRING))
	{
		PARTERRS(template, part, "expected: literal pathname\n");
		return -1;
	}
	if(!liquify_load(template->env, expr->root.right->text))
	{
		PARTERR(template, part, "failed to load included template '%s'\n", expr->root.right->text);
		return -1;
	}
	return 0;
}

int
liquify_tag_include_(LIQUIFYCTX *ctx, struct liquify_part *part)
{
	char *buf;

	if(ctx->tpl->env->depth >= MAX_INCLUDE_DEPTH)
	{
		liquify_emit_str(ctx, "[refusing to include '");
		liquify_emit_str(ctx, part->d.tag.pfirst->expr.root.right->text);
		liquify_emit_str(ctx, "' because the maximum inclusion depth has been reached]");
		return -1;
	}
	ctx->tpl->env->depth++;
	buf = liquify_apply_name(ctx->tpl->env, part->d.tag.pfirst->expr.root.right->text, ctx->dict);
	ctx->tpl->env->depth--;
	if(buf)
	{
		liquify_emit_str(ctx, buf);
		free(buf);
	}
	else
	{
		liquify_emit_str(ctx, "[failed to include '");
		liquify_emit_str(ctx, part->d.tag.pfirst->expr.root.right->text);
		liquify_emit_str(ctx, "']");
	}
	return 0;
}
