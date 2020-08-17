/* power-mgr.c */
#include <assert.h>
#include <flux/core.h>
#include <jansson.h>
#include <stdint.h>

#include "variorum.h"

static uint32_t rank, size;
#define MY_MOD_NAME "reduce"
const char default_service_name[] = MY_MOD_NAME;
//static const int NOFLAGS=0;

/* Here's the algorithm:
 *
 * Ranks from size/2 .. size-1 send a message to dag[UPSTREAM].
 * Ranks from 1 .. (size/2)-1 listen for messages from ranks rank*2 and (if needed) (rank*2)+1.  
 * 	They then combine the samples with their own sample and forward the new message to dag[UPSTREAM].
 * Rank 0 listens for messages from ranks 1 and 2, combines them as usually, then writes the summary
 * 	out to the kvs.
 *
 * To do this, all ranks take their measurements when the timer goes off.  Only ranks size/2 .. size-1
 * 	then send a message.
 * All of the other combining and message sending happens in the message handler.
 */

enum{
	UPSTREAM=0,
	DOWNSTREAM1,
	DOWNSTREAM2,
	MAX_NODE_EDGES,
	MAX_CHILDREN=2,
};

static int dag[MAX_NODE_EDGES];



static uint32_t g_sample = 0;		// sample count
//static uint32_t g_value = 10;		// Whatever it is we're measuring.
static double g_value = 0.0;		// Whatever it is we're measuring.

void reduce_ping_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg){
	static uint32_t _sample[MAX_CHILDREN] = {0,0};
	//static uint32_t _value[MAX_CHILDREN] = {0,0};
	static double _value[MAX_CHILDREN] = {0,0};

	uint32_t sender, in_sample; 
    double in_value;

	// Crack open the message and store off what we need.
	assert( flux_request_unpack (msg, NULL, "{s:i s:i s:f}", "sender", &sender, "sample", &in_sample, "value", &in_value) >= 0 );
	_sample[ sender%2 ] = in_sample;
	_value[ sender%2 ] = in_value;
	flux_log(h, LOG_CRIT, "QQQ %s:%d Rank %d received reduce.ping message from %d sample=%d value=%lf.\n", 
			__FILE__, __LINE__, rank, sender, in_sample, in_value);

	//FIXME Need to handle the case where we have an odd number of ranks.
	if( rank==0 ||_sample[0] == _sample[1] ){
		// We have both samples, combine with ours and push upstream.
		if( rank > 0 ){
			flux_future_t *f = flux_rpc_pack (
				h, 				// flux_t *h
				"reduce.ping", 			// char *topic
				dag[UPSTREAM],			// uint32_t nodeid (FLUX_NODEID_ANY, FLUX_NODEID_UPSTREAM, or a flux instance rank)
				FLUX_RPC_NORESPONSE,		// int flags (FLUX_RPC_NORESPONSE, FLUX_RPC_STREAMING, or NOFLAGS)
				"{s:i s:i s:f}", "sender", rank, "sample", g_sample, "value", g_value + _value[0] + _value[1]);	// const char *fmt, ...
				assert(f);
			flux_future_destroy(f);
		}
        else { 
			flux_log(h, LOG_CRIT, "QQQ %s:%d Rank %d received reduce.ping message sample=%d value=%lf.\n", 
					__FILE__, 
					__LINE__, 
					rank, 
					g_sample, 
					g_value + _value[0] + _value[1] );
	    }
	}

    // If Rank is 0, create the KVS transaction, now that we have the value.
    if (rank == 0) {
        int rc; 
        char kvs_key[50];
        char kvs_val[50];
 
        // Allocate the kvs transaction
        flux_kvs_txn_t *kvs_txn = flux_kvs_txn_create();
        assert( NULL != kvs_txn );

        // Create an entry from the kvs; 0 indicates NO_FLAGS
        sprintf(kvs_key, "job_power.sample.%d",g_sample);
        sprintf(kvs_val, "%lf",g_value + _value[0] + _value[1]);

        rc = flux_kvs_txn_put( kvs_txn, 0, kvs_key, kvs_val);
        assert( -1 != rc );

        // Commit the key+value.
        flux_future_t *kvs_future = flux_kvs_commit( h, NULL, 0, kvs_txn );
        assert( NULL != kvs_future );

        // Wait for confirmation
        rc = flux_future_wait_for( kvs_future, 10.0 );
        assert( rc != -1 );

        // Destroy our future.
        flux_future_destroy( kvs_future ); 
    }

}

static void timer_handler( flux_reactor_t *r, flux_watcher_t *w, int revents, void* arg ){

	static int initialized = 0;

    int ret; 
    json_t *power_obj  = json_object(); 
    double power_node; 

    if( initialized <= 5){
        initialized++;
	    flux_t *h = (flux_t*)arg;

	    // Go off and take your measurement.  
        ret = variorum_get_node_power_json(power_obj);
        if (ret != 0)                                                                  
            flux_log(h, LOG_CRIT, "JSON: get node power failed!\n");           

        power_node = json_real_value(json_object_get(power_obj, "power_node"));

	    g_sample++;
        // Instantaneous, no accumulation yet. 
	    g_value = power_node; 

        //flux_log(h, LOG_CRIT, "JSON: rank %d power is %lf\n", rank, power_node); 

	    // Then....
    	if( rank >= size/2 ){
		    // Just send the message.  These ranks don't do any combining.
	    	flux_log(h, LOG_CRIT, "!!! %s:%d LEAF rank %d (size=%d).\n", __FILE__, __LINE__, rank, size);
	    	flux_future_t *f = flux_rpc_pack (
		    	h, 				// flux_t *h
		    	"reduce.ping", 			// char *topic
		    	dag[UPSTREAM],			// uint32_t nodeid (FLUX_NODEID_ANY, FLUX_NODEID_UPSTREAM, or a flux instance rank)
		    	FLUX_RPC_NORESPONSE,		// int flags (FLUX_RPC_NORESPONSE, FLUX_RPC_STREAMING, or NOFLAGS)
		    	"{s:i s:i s:f}", "sender", rank, "sample", g_sample++, "value", g_value);	// const char *fmt, ...
		    	assert(f);
		    	flux_future_destroy(f);
        }
    }

        //Clean up JSON obj
        json_decref(power_obj); 
}



static const struct flux_msg_handler_spec htab[] = { 
    //int typemask;           const char *topic_glob;	flux_msg_handler_f cb;  uint32_t rolemask;
    { FLUX_MSGTYPE_REQUEST,   "reduce.ping",    	reduce_ping_cb, 	0 },
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main (flux_t *h, int argc, char **argv){

	flux_get_rank(h, &rank);
	flux_get_size(h, &size);

	//flux_log(h, LOG_CRIT, "QQQ %s:%d Hello from rank %d of %d.\n", __FILE__, __LINE__, rank, size);

	// We don't have easy access to the topology of the underlying flux network, so we'll set up
	// an overlay instead.  
	dag[UPSTREAM]    = (rank==0)       ? -1 : rank / 2;
	dag[DOWNSTREAM1] = (rank > size/2) ? -1 : rank * 2;
	dag[DOWNSTREAM2] = (rank > size/2) ? -1 : rank * 2 + 1;
	if( rank==size/2 && size%2 ){
		// If we have an odd size then rank size/2 only gets a single child.
		dag[DOWNSTREAM2] = -1;
	}

	flux_msg_handler_t **handlers = NULL;

	// Let all ranks set this up.
	assert( flux_msg_handler_addvec (h, htab, NULL, &handlers) >= 0 );

	// All ranks set a handler for the timer.
	flux_watcher_t* timer_watch_p = flux_timer_watcher_create( flux_get_reactor(h), 2.0, 2.0, timer_handler, h); 
	assert( timer_watch_p );
	flux_watcher_start( timer_watch_p );	

	// Run!
	assert( flux_reactor_run (flux_get_reactor (h), 0) >= 0 );

	// On unload, shutdown the handlers.
	if( rank==0 ){
		flux_msg_handler_delvec (handlers);
	}

	return 0;
}

MOD_NAME (MY_MOD_NAME);

