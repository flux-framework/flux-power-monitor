/* power-mgr.c */
#include <assert.h>
#include <flux/core.h>
#include <jansson.h>
#include <stdint.h>

static uint32_t rank, size;
#define MY_MOD_NAME "reduce"
const char default_service_name[] = MY_MOD_NAME;

void reduce_ping_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg){
	int i;
	flux_log(h, LOG_CRIT, "QQQ %s:%d Rank %d received a rpctest.incr query.\n", __FILE__, __LINE__, rank );
	assert( flux_request_unpack (msg, NULL, "{s:i}", "n", &i) >= 0 );
	assert( flux_respond_pack (h, msg, "{s:i}", "n", i + 1) >= 0 );
	flux_log(h, LOG_CRIT, "QQQ %s:%d Rank %d responded to a rpctest.incr query.\n", __FILE__, __LINE__, rank );

}

static void timer_handler( flux_reactor_t *r, flux_watcher_t *w, int revents, void* arg ){

        static int initialized = 0;
        flux_t *h = (flux_t*)arg;

        if( !initialized ){
		initialized = 1;
		flux_log(h, LOG_CRIT, "QQQ %s:%d rank %d of %d timer notification.\n", __FILE__, __LINE__, rank, size);
	}
}



static const struct flux_msg_handler_spec htab[] = { 
    //int typemask;           const char *topic_glob;	flux_msg_handler_f cb;  uint32_t rolemask;
    { FLUX_MSGTYPE_REQUEST,   "reduce.ping",    	reduce_ping_cb, 	0 },
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main (flux_t *h, int argc, char **argv){

	flux_get_rank(h, &rank);
	flux_get_size(h, &size);

	flux_log(h, LOG_CRIT, "QQQ %s:%d Hello from rank %d of %d.\n", __FILE__, __LINE__, rank, size);

	flux_msg_handler_t **handlers = NULL;

	// All ranks set a handler for the reduce.ping message.
	assert( flux_msg_handler_addvec (h, htab, NULL, &handlers) >= 0 );

	// All ranks set a handler for the timer.
	flux_watcher_t* timer_watch_p = flux_timer_watcher_create( flux_get_reactor(h), 3.0, 3.0, timer_handler, h); 
	assert( timer_watch_p );
	flux_watcher_start( timer_watch_p );	

	// Run!
	assert( flux_reactor_run (flux_get_reactor (h), 0) >= 0 );

	// On unload, shutdown the handlers.
	flux_msg_handler_delvec (handlers);

	return 0;
}

MOD_NAME (MY_MOD_NAME);

#if 0
/* increment integer and send it back */
void rpctest_incr_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg)
{
	int i;
	flux_log(h, LOG_CRIT, "QQQ %s:%d Rank %d received a rpctest.incr query.\n", __FILE__, __LINE__, rank );
	assert( flux_request_unpack (msg, NULL, "{s:i}", "n", &i) >= 0 );
	assert( flux_respond_pack (h, msg, "{s:i}", "n", i + 1) >= 0 );
	flux_log(h, LOG_CRIT, "QQQ %s:%d Rank %d responded to a rpctest.incr query.\n", __FILE__, __LINE__, rank );

}

void timer_handler( flux_reactor_t *r, flux_watcher_t *w, int revents, void* arg ){

	int i;
        static int initialized = 0;
        flux_t *h = (flux_t*)arg;
	// Confirm we really need this.
        flux_get_rank(h, &rank);
        flux_get_size(h, &size);

        if( !initialized ){
		initialized = 1;
		// discards the future, probably a leak.
		flux_log(h, LOG_CRIT, "QQQ %s:%d rank %d preparing to make rpctest.incr query.\n", __FILE__, __LINE__, rank);
		flux_future_t *f = flux_rpc_pack (h, "happiness.incr", FLUX_NODEID_UPSTREAM, 0, "{s:i}", "n", 107);
		assert(f);
		int rc = flux_rpc_get_unpack (f, "{s:i}", "n", &i);
		if( rc == -1 ){
			flux_log_error( h, "flux_rpc_get_unpack() exploded." );
		}
		flux_future_destroy(f);
		flux_log(h, LOG_CRIT, "QQQ %s:%d rank %d received response n=%d.\n", __FILE__, __LINE__, rank, i);
	}
}


#endif
