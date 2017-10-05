/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */
#include <ngx_config.h>
#include <ngx_core.h>
#pragma hdrstop
//#include <ngx_http.h>

struct ngx_http_limit_req_node_t {
	u_char color;
	u_char dummy;
	u_short len;
	ngx_queue_t queue;
	ngx_msec_t last;
	/* integer value, 1 corresponds to 0.001 r/s */
	ngx_uint_t excess;
	ngx_uint_t count;
	u_char data[1];
};

struct ngx_http_limit_req_shctx_t {
	ngx_rbtree_t rbtree;
	ngx_rbtree_node_t sentinel;
	ngx_queue_t queue;
};

struct ngx_http_limit_req_ctx_t {
	ngx_http_limit_req_shctx_t  * sh;
	ngx_slab_pool_t   * shpool;
	/* integer value, 1 corresponds to 0.001 r/s */
	ngx_uint_t rate;
	ngx_http_complex_value_t key;
	ngx_http_limit_req_node_t * node;
};

struct ngx_http_limit_req_limit_t {
	ngx_shm_zone_t * shm_zone;
	// integer value, 1 corresponds to 0.001 r/s 
	ngx_uint_t burst;
	ngx_uint_t nodelay; // unsigned  nodelay:1 
};

struct ngx_http_limit_req_conf_t {
	ngx_array_t limits;
	ngx_uint_t limit_log_level;
	ngx_uint_t delay_log_level;
	ngx_uint_t status_code;
};

static void ngx_http_limit_req_delay(ngx_http_request_t * r);
static ngx_int_t ngx_http_limit_req_lookup(ngx_http_limit_req_limit_t * limit, ngx_uint_t hash, ngx_str_t * key, ngx_uint_t * ep, ngx_uint_t account);
static ngx_msec_t ngx_http_limit_req_account(ngx_http_limit_req_limit_t * limits, ngx_uint_t n, ngx_uint_t * ep, ngx_http_limit_req_limit_t ** limit);
static void ngx_http_limit_req_expire(ngx_http_limit_req_ctx_t * ctx, ngx_uint_t n);
static void * ngx_http_limit_req_create_conf(ngx_conf_t * cf);
static char * ngx_http_limit_req_merge_conf(ngx_conf_t * cf, void * parent, void * child);
static const char * ngx_http_limit_req_zone(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf); // F_SetHandler
static const char * ngx_http_limit_req(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf); // F_SetHandler
static ngx_int_t ngx_http_limit_req_init(ngx_conf_t * cf);

static ngx_conf_enum_t ngx_http_limit_req_log_levels[] = {
	{ ngx_string("info"), NGX_LOG_INFO },
	{ ngx_string("notice"), NGX_LOG_NOTICE },
	{ ngx_string("warn"), NGX_LOG_WARN },
	{ ngx_string("error"), NGX_LOG_ERR },
	{ ngx_null_string, 0 }
};

static ngx_conf_num_bounds_t ngx_http_limit_req_status_bounds = {
	ngx_conf_check_num_bounds, 400, 599
};

static ngx_command_t ngx_http_limit_req_commands[] = {
	{ ngx_string("limit_req_zone"), NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE3,
	  ngx_http_limit_req_zone, 0, 0, NULL },
	{ ngx_string("limit_req"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE123,
	  ngx_http_limit_req, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },
	{ ngx_string("limit_req_log_level"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_enum_slot, NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_limit_req_conf_t, limit_log_level), &ngx_http_limit_req_log_levels },
	{ ngx_string("limit_req_status"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_num_slot, NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_limit_req_conf_t, status_code), &ngx_http_limit_req_status_bounds },
	ngx_null_command
};

static ngx_http_module_t ngx_http_limit_req_module_ctx = {
	NULL,                              /* preconfiguration */
	ngx_http_limit_req_init,           /* postconfiguration */
	NULL,                              /* create main configuration */
	NULL,                              /* init main configuration */
	NULL,                              /* create server configuration */
	NULL,                              /* merge server configuration */
	ngx_http_limit_req_create_conf,    /* create location configuration */
	ngx_http_limit_req_merge_conf      /* merge location configuration */
};

ngx_module_t ngx_http_limit_req_module = {
	NGX_MODULE_V1,
	&ngx_http_limit_req_module_ctx,    /* module context */
	ngx_http_limit_req_commands,       /* module directives */
	NGX_HTTP_MODULE,                   /* module type */
	NULL,                              /* init master */
	NULL,                              /* init module */
	NULL,                              /* init process */
	NULL,                              /* init thread */
	NULL,                              /* exit thread */
	NULL,                              /* exit process */
	NULL,                              /* exit master */
	NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_limit_req_handler(ngx_http_request_t * r)
{
	uint32_t hash;
	ngx_str_t key;
	ngx_int_t rc;
	ngx_uint_t n, excess;
	ngx_msec_t delay;
	ngx_http_limit_req_ctx_t  * ctx;
	ngx_http_limit_req_conf_t * lrcf;
	ngx_http_limit_req_limit_t  * limit, * limits;
	if(r->main->limit_req_set) {
		return NGX_DECLINED;
	}
	lrcf = (ngx_http_limit_req_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_limit_req_module);
	limits = (ngx_http_limit_req_limit_t *)lrcf->limits.elts;
	excess = 0;
	rc = NGX_DECLINED;
#if (NGX_SUPPRESS_WARN)
	limit = NULL;
#endif
	for(n = 0; n < lrcf->limits.nelts; n++) {
		limit = &limits[n];
		ctx = (ngx_http_limit_req_ctx_t *)limit->shm_zone->data;
		if(ngx_http_complex_value(r, &ctx->key, &key) != NGX_OK) {
			return NGX_HTTP_INTERNAL_SERVER_ERROR;
		}
		if(key.len == 0) {
			continue;
		}
		if(key.len > 65535) {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "the value of the \"%V\" key is more than 65535 bytes: \"%V\"", &ctx->key.value, &key);
			continue;
		}
		hash = ngx_crc32_short(key.data, key.len);
		ngx_shmtx_lock(&ctx->shpool->mutex);
		rc = ngx_http_limit_req_lookup(limit, hash, &key, &excess, (n == lrcf->limits.nelts - 1));
		ngx_shmtx_unlock(&ctx->shpool->mutex);
		ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "limit_req[%ui]: %i %ui.%03ui", n, rc, excess / 1000, excess % 1000);
		if(rc != NGX_AGAIN) {
			break;
		}
	}
	if(rc == NGX_DECLINED) {
		return NGX_DECLINED;
	}
	r->main->limit_req_set = 1;
	if(rc == NGX_BUSY || rc == NGX_ERROR) {
		if(rc == NGX_BUSY) {
			ngx_log_error(lrcf->limit_log_level, r->connection->log, 0, "limiting requests, excess: %ui.%03ui by zone \"%V\"",
			    excess / 1000, excess % 1000, &limit->shm_zone->shm.name);
		}
		while(n--) {
			ctx = (ngx_http_limit_req_ctx_t *)limits[n].shm_zone->data;
			if(ctx->node) {
				ngx_shmtx_lock(&ctx->shpool->mutex);
				ctx->node->count--;
				ngx_shmtx_unlock(&ctx->shpool->mutex);
				ctx->node = NULL;
			}
		}
		return lrcf->status_code;
	}
	/* rc == NGX_AGAIN || rc == NGX_OK */
	if(rc == NGX_AGAIN) {
		excess = 0;
	}
	delay = ngx_http_limit_req_account(limits, n, &excess, &limit);
	if(!delay) {
		return NGX_DECLINED;
	}
	ngx_log_error(lrcf->delay_log_level, r->connection->log, 0, "delaying request, excess: %ui.%03ui, by zone \"%V\"", excess / 1000, excess % 1000, &limit->shm_zone->shm.name);
	if(ngx_handle_read_event(r->connection->P_EvRd, 0) != NGX_OK) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	r->read_event_handler = ngx_http_test_reading;
	r->write_event_handler = ngx_http_limit_req_delay;
	r->connection->P_EvWr->delayed = 1;
	ngx_add_timer(r->connection->P_EvWr, delay);
	return NGX_AGAIN;
}

static void ngx_http_limit_req_delay(ngx_http_request_t * r)
{
	ngx_event_t  * wev;
	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "limit_req delay");
	wev = r->connection->P_EvWr;
	if(wev->delayed) {
		if(ngx_handle_write_event(wev, 0) != NGX_OK) {
			ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		}
		return;
	}
	if(ngx_handle_read_event(r->connection->P_EvRd, 0) != NGX_OK) {
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	r->read_event_handler = ngx_http_block_reading;
	r->write_event_handler = ngx_http_core_run_phases;
	ngx_http_core_run_phases(r);
}

static void ngx_http_limit_req_rbtree_insert_value(ngx_rbtree_node_t * temp, ngx_rbtree_node_t * node, ngx_rbtree_node_t * sentinel)
{
	ngx_rbtree_node_t ** p;
	ngx_http_limit_req_node_t * lrn, * lrnt;
	for(;; ) {
		if(node->key < temp->key) {
			p = &temp->left;
		}
		else if(node->key > temp->key) {
			p = &temp->right;
		}
		else { /* node->key == temp->key */
			lrn = (ngx_http_limit_req_node_t*)&node->color;
			lrnt = (ngx_http_limit_req_node_t*)&temp->color;
			p = (ngx_memn2cmp(lrn->data, lrnt->data, lrn->len, lrnt->len) < 0) ? &temp->left : &temp->right;
		}
		if(*p == sentinel) {
			break;
		}
		temp = *p;
	}
	*p = node;
	node->parent = temp;
	node->left = sentinel;
	node->right = sentinel;
	ngx_rbt_red(node);
}

static ngx_int_t ngx_http_limit_req_lookup(ngx_http_limit_req_limit_t * limit, ngx_uint_t hash, ngx_str_t * key, ngx_uint_t * ep, ngx_uint_t account)
{
	size_t size;
	ngx_int_t rc, excess;
	ngx_msec_int_t ms;
	ngx_http_limit_req_node_t  * lr;
	ngx_msec_t now = ngx_current_msec;
	ngx_http_limit_req_ctx_t * ctx = (ngx_http_limit_req_ctx_t *)limit->shm_zone->data;
	ngx_rbtree_node_t * node = ctx->sh->rbtree.root;
	ngx_rbtree_node_t * sentinel = ctx->sh->rbtree.sentinel;
	while(node != sentinel) {
		if(hash < node->key) {
			node = node->left;
			continue;
		}
		if(hash > node->key) {
			node = node->right;
			continue;
		}
		/* hash == node->key */
		lr = (ngx_http_limit_req_node_t*)&node->color;
		rc = ngx_memn2cmp(key->data, lr->data, key->len, (size_t)lr->len);
		if(rc == 0) {
			ngx_queue_remove(&lr->queue);
			ngx_queue_insert_head(&ctx->sh->queue, &lr->queue);
			ms = (ngx_msec_int_t)(now - lr->last);
			excess = lr->excess - ctx->rate * ngx_abs(ms) / 1000 + 1000;
			if(excess < 0) {
				excess = 0;
			}
			*ep = excess;
			if((ngx_uint_t)excess > limit->burst) {
				return NGX_BUSY;
			}
			if(account) {
				lr->excess = excess;
				lr->last = now;
				return NGX_OK;
			}
			lr->count++;
			ctx->node = lr;
			return NGX_AGAIN;
		}
		node = (rc < 0) ? node->left : node->right;
	}
	*ep = 0;
	size = offsetof(ngx_rbtree_node_t, color) + offsetof(ngx_http_limit_req_node_t, data) + key->len;
	ngx_http_limit_req_expire(ctx, 1);
	node = (ngx_rbtree_node_t *)ngx_slab_alloc_locked(ctx->shpool, size);
	if(!node) {
		ngx_http_limit_req_expire(ctx, 0);
		node = (ngx_rbtree_node_t *)ngx_slab_alloc_locked(ctx->shpool, size);
		if(!node) {
			ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, "could not allocate node%s", ctx->shpool->log_ctx);
			return NGX_ERROR;
		}
	}
	node->key = hash;
	lr = (ngx_http_limit_req_node_t*)&node->color;
	lr->len = (u_short)key->len;
	lr->excess = 0;
	memcpy(lr->data, key->data, key->len);
	ngx_rbtree_insert(&ctx->sh->rbtree, node);
	ngx_queue_insert_head(&ctx->sh->queue, &lr->queue);
	if(account) {
		lr->last = now;
		lr->count = 0;
		return NGX_OK;
	}
	lr->last = 0;
	lr->count = 1;
	ctx->node = lr;
	return NGX_AGAIN;
}

static ngx_msec_t ngx_http_limit_req_account(ngx_http_limit_req_limit_t * limits, ngx_uint_t n, ngx_uint_t * ep, ngx_http_limit_req_limit_t ** limit)
{
	ngx_msec_t now, delay, max_delay;
	ngx_msec_int_t ms;
	ngx_http_limit_req_ctx_t * ctx;
	ngx_http_limit_req_node_t  * lr;
	ngx_int_t excess = *ep;
	if(excess == 0 || (*limit)->nodelay) {
		max_delay = 0;
	}
	else {
		ctx = (ngx_http_limit_req_ctx_t *)(*limit)->shm_zone->data;
		max_delay = excess * 1000 / ctx->rate;
	}
	while(n--) {
		ctx = (ngx_http_limit_req_ctx_t *)limits[n].shm_zone->data;
		lr = ctx->node;
		if(lr == NULL) {
			continue;
		}
		ngx_shmtx_lock(&ctx->shpool->mutex);

		now = ngx_current_msec;
		ms = (ngx_msec_int_t)(now - lr->last);
		excess = lr->excess - ctx->rate * ngx_abs(ms) / 1000 + 1000;
		if(excess < 0) {
			excess = 0;
		}
		lr->last = now;
		lr->excess = excess;
		lr->count--;
		ngx_shmtx_unlock(&ctx->shpool->mutex);
		ctx->node = NULL;
		if(limits[n].nodelay) {
			continue;
		}
		delay = excess * 1000 / ctx->rate;
		if(delay > max_delay) {
			max_delay = delay;
			*ep = excess;
			*limit = &limits[n];
		}
	}
	return max_delay;
}

static void ngx_http_limit_req_expire(ngx_http_limit_req_ctx_t * ctx, ngx_uint_t n)
{
	ngx_int_t excess;
	ngx_queue_t  * q;
	ngx_msec_int_t ms;
	ngx_rbtree_node_t   * node;
	ngx_http_limit_req_node_t  * lr;
	ngx_msec_t now = ngx_current_msec;
	/*
	 * n == 1 deletes one or two zero rate entries
	 * n == 0 deletes oldest entry by force
	 *        and one or two zero rate entries
	 */

	while(n < 3) {
		if(ngx_queue_empty(&ctx->sh->queue)) {
			return;
		}
		q = ngx_queue_last(&ctx->sh->queue);
		lr = ngx_queue_data(q, ngx_http_limit_req_node_t, queue);
		if(lr->count) {
			/*
			 * There is not much sense in looking further,
			 * because we bump nodes on the lookup stage.
			 */
			return;
		}
		if(n++ != 0) {
			ms = (ngx_msec_int_t)(now - lr->last);
			ms = ngx_abs(ms);
			if(ms < 60000) {
				return;
			}
			excess = lr->excess - ctx->rate * ms / 1000;
			if(excess > 0) {
				return;
			}
		}
		ngx_queue_remove(q);
		node = (ngx_rbtree_node_t*)((u_char*)lr - offsetof(ngx_rbtree_node_t, color));
		ngx_rbtree_delete(&ctx->sh->rbtree, node);
		ngx_slab_free_locked(ctx->shpool, node);
	}
}

static ngx_int_t ngx_http_limit_req_init_zone(ngx_shm_zone_t * shm_zone, void * data)
{
	ngx_http_limit_req_ctx_t  * octx = (ngx_http_limit_req_ctx_t *)data;
	size_t len;
	ngx_http_limit_req_ctx_t  * ctx = (ngx_http_limit_req_ctx_t *)shm_zone->data;
	if(octx) {
		if(ctx->key.value.len != octx->key.value.len || ngx_strncmp(ctx->key.value.data, octx->key.value.data, ctx->key.value.len) != 0) {
			ngx_log_error(NGX_LOG_EMERG, shm_zone->shm.log, 0, "limit_req \"%V\" uses the \"%V\" key while previously it used the \"%V\" key",
			    &shm_zone->shm.name, &ctx->key.value, &octx->key.value);
			return NGX_ERROR;
		}
		ctx->sh = octx->sh;
		ctx->shpool = octx->shpool;
		return NGX_OK;
	}
	ctx->shpool = (ngx_slab_pool_t*)shm_zone->shm.addr;
	if(shm_zone->shm.exists) {
		ctx->sh = (ngx_http_limit_req_shctx_t *)ctx->shpool->data;
		return NGX_OK;
	}
	ctx->sh = (ngx_http_limit_req_shctx_t *)ngx_slab_alloc(ctx->shpool, sizeof(ngx_http_limit_req_shctx_t));
	if(ctx->sh == NULL) {
		return NGX_ERROR;
	}
	ctx->shpool->data = ctx->sh;
	ngx_rbtree_init(&ctx->sh->rbtree, &ctx->sh->sentinel, ngx_http_limit_req_rbtree_insert_value);
	ngx_queue_init(&ctx->sh->queue);
	len = sizeof(" in limit_req zone \"\"") + shm_zone->shm.name.len;
	ctx->shpool->log_ctx = (u_char*)ngx_slab_alloc(ctx->shpool, len);
	if(ctx->shpool->log_ctx == NULL) {
		return NGX_ERROR;
	}
	ngx_sprintf(ctx->shpool->log_ctx, " in limit_req zone \"%V\"%Z", &shm_zone->shm.name);
	ctx->shpool->log_nomem = 0;
	return NGX_OK;
}

static void * ngx_http_limit_req_create_conf(ngx_conf_t * cf)
{
	ngx_http_limit_req_conf_t  * conf = (ngx_http_limit_req_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_limit_req_conf_t));
	if(!conf) {
		return NULL;
	}
	/*
	 * set by ngx_pcalloc():
	 *
	 *     conf->limits.elts = NULL;
	 */
	conf->limit_log_level = NGX_CONF_UNSET_UINT;
	conf->status_code = NGX_CONF_UNSET_UINT;
	return conf;
}

static char * ngx_http_limit_req_merge_conf(ngx_conf_t * cf, void * parent, void * child)
{
	ngx_http_limit_req_conf_t * prev = (ngx_http_limit_req_conf_t *)parent;
	ngx_http_limit_req_conf_t * conf = (ngx_http_limit_req_conf_t *)child;
	if(conf->limits.elts == NULL) {
		conf->limits = prev->limits;
	}
	ngx_conf_merge_uint_value(conf->limit_log_level, prev->limit_log_level, NGX_LOG_ERR);
	conf->delay_log_level = (conf->limit_log_level == NGX_LOG_INFO) ? NGX_LOG_INFO : conf->limit_log_level + 1;
	ngx_conf_merge_uint_value(conf->status_code, prev->status_code, NGX_HTTP_SERVICE_UNAVAILABLE);
	return NGX_CONF_OK;
}

static const char * ngx_http_limit_req_zone(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf) // F_SetHandler
{
	u_char * p;
	size_t len;
	ssize_t size;
	ngx_str_t name, s;
	ngx_int_t rate, scale;
	ngx_uint_t i;
	ngx_shm_zone_t * shm_zone;
	ngx_http_compile_complex_value_t ccv;
	ngx_str_t * value = (ngx_str_t*)cf->args->elts;
	ngx_http_limit_req_ctx_t * ctx = (ngx_http_limit_req_ctx_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_limit_req_ctx_t));
	if(!ctx) {
		return NGX_CONF_ERROR;
	}
	memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
	ccv.cf = cf;
	ccv.value = &value[1];
	ccv.complex_value = &ctx->key;
	if(ngx_http_compile_complex_value(&ccv) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	size = 0;
	rate = 1;
	scale = 1;
	name.len = 0;
	for(i = 2; i < cf->args->nelts; i++) {
		if(ngx_strncmp(value[i].data, "zone=", 5) == 0) {
			name.data = value[i].data + 5;
			p = (u_char*)ngx_strchr(name.data, ':');
			if(!p) {
				ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid zone size \"%V\"", &value[i]);
				return NGX_CONF_ERROR;
			}
			name.len = p - name.data;
			s.data = p + 1;
			s.len = value[i].data + value[i].len - s.data;
			size = ngx_parse_size(&s);
			if(size == NGX_ERROR) {
				ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid zone size \"%V\"", &value[i]);
				return NGX_CONF_ERROR;
			}
			if(size < (ssize_t)(8 * ngx_pagesize)) {
				ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "zone \"%V\" is too small", &value[i]);
				return NGX_CONF_ERROR;
			}
			continue;
		}
		if(ngx_strncmp(value[i].data, "rate=", 5) == 0) {
			len = value[i].len;
			p = value[i].data + len - 3;
			if(ngx_strncmp(p, "r/s", 3) == 0) {
				scale = 1;
				len -= 3;
			}
			else if(ngx_strncmp(p, "r/m", 3) == 0) {
				scale = 60;
				len -= 3;
			}
			rate = ngx_atoi(value[i].data + 5, len - 5);
			if(rate <= 0) {
				ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid rate \"%V\"", &value[i]);
				return NGX_CONF_ERROR;
			}
			continue;
		}
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[i]);
		return NGX_CONF_ERROR;
	}
	if(name.len == 0) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "\"%V\" must have \"zone\" parameter", &cmd->Name);
		return NGX_CONF_ERROR;
	}
	ctx->rate = rate * 1000 / scale;
	shm_zone = ngx_shared_memory_add(cf, &name, size, &ngx_http_limit_req_module);
	if(shm_zone == NULL) {
		return NGX_CONF_ERROR;
	}
	if(shm_zone->data) {
		ctx = (ngx_http_limit_req_ctx_t *)shm_zone->data;
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V \"%V\" is already bound to key \"%V\"", &cmd->Name, &name, &ctx->key.value);
		return NGX_CONF_ERROR;
	}
	shm_zone->F_Init = ngx_http_limit_req_init_zone;
	shm_zone->data = ctx;
	return NGX_CONF_OK;
}

static const char * ngx_http_limit_req(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf) // F_SetHandler
{
	ngx_http_limit_req_conf_t  * lrcf = (ngx_http_limit_req_conf_t *)conf;
	ngx_str_t s;
	ngx_uint_t i;
	ngx_http_limit_req_limit_t  * limit, * limits;
	ngx_str_t * value = (ngx_str_t*)cf->args->elts;
	ngx_shm_zone_t * shm_zone = NULL;
	ngx_int_t burst = 0;
	ngx_uint_t nodelay = 0;
	for(i = 1; i < cf->args->nelts; i++) {
		if(ngx_strncmp(value[i].data, "zone=", 5) == 0) {
			s.len = value[i].len - 5;
			s.data = value[i].data + 5;
			shm_zone = ngx_shared_memory_add(cf, &s, 0, &ngx_http_limit_req_module);
			if(shm_zone == NULL) {
				return NGX_CONF_ERROR;
			}
			continue;
		}
		if(ngx_strncmp(value[i].data, "burst=", 6) == 0) {
			burst = ngx_atoi(value[i].data + 6, value[i].len - 6);
			if(burst <= 0) {
				ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid burst rate \"%V\"", &value[i]);
				return NGX_CONF_ERROR;
			}
			continue;
		}
		if(ngx_strcmp(value[i].data, "nodelay") == 0) {
			nodelay = 1;
			continue;
		}
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[i]);
		return NGX_CONF_ERROR;
	}
	if(shm_zone == NULL) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "\"%V\" must have \"zone\" parameter", &cmd->Name);
		return NGX_CONF_ERROR;
	}
	limits = (ngx_http_limit_req_limit_t *)lrcf->limits.elts;
	if(limits == NULL) {
		if(ngx_array_init(&lrcf->limits, cf->pool, 1, sizeof(ngx_http_limit_req_limit_t)) != NGX_OK) {
			return NGX_CONF_ERROR;
		}
	}
	for(i = 0; i < lrcf->limits.nelts; i++) {
		if(shm_zone == limits[i].shm_zone) {
			return "is duplicate";
		}
	}
	limit = (ngx_http_limit_req_limit_t *)ngx_array_push(&lrcf->limits);
	if(limit == NULL) {
		return NGX_CONF_ERROR;
	}
	limit->shm_zone = shm_zone;
	limit->burst = burst * 1000;
	limit->nodelay = nodelay;
	return NGX_CONF_OK;
}

static ngx_int_t ngx_http_limit_req_init(ngx_conf_t * cf)
{
	ngx_http_core_main_conf_t * cmcf = (ngx_http_core_main_conf_t*)ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	ngx_http_handler_pt * h = (ngx_http_handler_pt*)ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
	if(h == NULL) {
		return NGX_ERROR;
	}
	*h = ngx_http_limit_req_handler;
	return NGX_OK;
}
