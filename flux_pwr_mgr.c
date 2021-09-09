/* power-mgr.c */
#include <assert.h>
#include <flux/core.h>
#include <jansson.h>
#include <stdint.h>
#include <unistd.h>

#include "variorum.h"

static uint32_t rank, size;
#define MY_MOD_NAME "flux_pwr_mgr"
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

static uint32_t sample_id = 0;		// sample counter

static double node_power_acc = 0.0;		// Node power accumulator
static double gpu_power_acc = 0.0;		// GPU power accumulator
static double cpu_power_acc = 0.0;		// CPU power accumulator
static double mem_power_acc = 0.0;   // Memory power accumulator

void flux_pwr_mgr_set_powercap_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg){

    int power_cap;                                                                      
    char *errmsg = "";                                                             
    int ret = 0; 
                                                                                
    /*  Use flux_msg_get_payload(3) to get the raw data and size                   
     *   of the request payload.                                                   
     *  For JSON payloads, also see flux_request_unpack().                         
     */                                                                            
    if (flux_request_unpack (msg, NULL, "{s:i}", "node", &power_cap) < 0) {                            
        flux_log_error (h, "flux_pwr_mgr_set_powercap_cb: flux_request_unpack");                       
        errmsg = "flux_request_unpack failed";                                    
        goto err;                                                                  
    }                                                                              
 
	flux_log(h, LOG_CRIT, "I received flux_pwr_mgr.set_powercap message of %d W. \n", power_cap);
 
    ret = variorum_cap_best_effort_node_power_limit(power_cap);                       
    if (ret != 0)                                                                  
    {                                                                              
        flux_log(h, LOG_CRIT, "Variorum set node power limit failed!\n");                                  
        return;                                                                
    }
                                                                              
    /*  Use flux_respond_raw(3) to include copy of payload in the response.        
     *  For JSON payloads, see flux_respond_pack(3).                               
     */                                                                       

    // We only get here if power capping succeeds.      
    if (flux_respond_pack (h, msg, "{s:i}", "node", power_cap) < 0) {                               
        flux_log_error (h, "flux_pwr_mgr_set_powercap_cb: flux_respond_pack");                           
        errmsg = "flux_respond_pack failed";                                        
        goto err;                                                                  
    }                                                                              
    return;                                                                        
err:                                                                               
    if (flux_respond_error (h, msg, errno, errmsg) < 0)                            
        flux_log_error (h, "flux_respond_error");               
}

void flux_pwr_mgr_collect_power_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg){
	static uint32_t _sample[MAX_CHILDREN] = {0,0};
	static double _node_value[MAX_CHILDREN] = {0,0};
	static double _gpu_value[MAX_CHILDREN] = {0,0};
	static double _cpu_value[MAX_CHILDREN] = {0,0};
	static double _mem_value[MAX_CHILDREN] = {0,0};

	uint32_t sender, in_sample; 
    double temp_1, temp_2, temp_3, temp_4;

    const char* recv_from_hostname;
    char my_hostname[256];
    gethostname(my_hostname, 256); 

	// Crack open the message and store off what we need.
	assert( flux_request_unpack (msg, NULL, "{s:i s:i s:s s:f s:f s:f s:f}", 
            "sender", &sender, "sample", &in_sample, "hostname", &recv_from_hostname, 
            "node_power", &temp_1, "gpu_power", &temp_2, "cpu_power", &temp_3, "mem_power", &temp_4) >= 0 );

	_sample[ sender%2 ] = in_sample;
	_node_value[ sender%2 ] = temp_1;
	_gpu_value[ sender%2 ] = temp_2;
	_cpu_value[ sender%2 ] = temp_3;
	_mem_value[ sender%2 ] = temp_4;
	
    flux_log(h, LOG_CRIT, "QQQ %s:%d Rank %d received flux_pwr_mgr.collect_power message from %d sample=%d node value=%lf and host %s.\n", 
			__FILE__, __LINE__, rank, sender, in_sample, temp_1, recv_from_hostname);

	if( rank==0 ||_sample[0] == _sample[1] ){
		// We have both samples, combine with ours and push upstream.
		if( rank > 0 ){
			flux_future_t *f = flux_rpc_pack (
				h, 				// flux_t *h
				"flux_pwr_mgr.collect_power", 			// char *topic
				dag[UPSTREAM],			// uint32_t nodeid (FLUX_NODEID_ANY, FLUX_NODEID_UPSTREAM, or a flux instance rank)
				FLUX_RPC_NORESPONSE,		// int flags (FLUX_RPC_NORESPONSE, FLUX_RPC_STREAMING, or NOFLAGS)
				"{s:i s:i s:s s:f s:f s:f s:f}", "sender", rank, "sample", sample_id, "hostname", my_hostname,  
                "node_power", node_power_acc + _node_value[0] + _node_value[1], 
                 "gpu_power", gpu_power_acc + _gpu_value[0] + _gpu_value[1], 
                 "cpu_power", cpu_power_acc + _cpu_value[0] + _cpu_value[1], 
                 "mem_power", mem_power_acc + _mem_value[0] + _mem_value[1]);
				assert(f);
			flux_future_destroy(f);
		}
        else { //Rank 0? 
			flux_log(h, LOG_CRIT, "ZERO %s:%d Rank %d has sample=%d node_value=%lf and host %s .\n", 
					__FILE__, 
					__LINE__, 
					rank, 
					sample_id, 
					node_power_acc + _node_value[0] + _node_value[1], my_hostname );
	    }
	}

    // If Rank is 0, create the KVS transaction, now that we have the value.
    if (rank == 0) {
        int rc; 
        char kvs_key[80];
        char kvs_val[80];
 
        // Allocate the kvs transaction
        flux_kvs_txn_t *kvs_txn = flux_kvs_txn_create();
        assert( NULL != kvs_txn );

        // Create an entry from the kvs; 0 indicates NO_FLAGS
        sprintf(kvs_key, "job_total_power.sample.%d",sample_id);
        sprintf(kvs_val, "%lf",node_power_acc + _node_value[0] + _node_value[1]);
        rc = flux_kvs_txn_put( kvs_txn, 0, kvs_key, kvs_val);
        assert( -1 != rc );

        sprintf(kvs_key, "job_gpu_power.sample.%d",sample_id);
        sprintf(kvs_val, "%lf",gpu_power_acc + _gpu_value[0] + _gpu_value[1]);
        rc = flux_kvs_txn_put( kvs_txn, 0, kvs_key, kvs_val);
        assert( -1 != rc );

        sprintf(kvs_key, "job_cpu_power.sample.%d",sample_id);
        sprintf(kvs_val, "%lf",cpu_power_acc + _cpu_value[0] + _cpu_value[1]);
        rc = flux_kvs_txn_put( kvs_txn, 0, kvs_key, kvs_val);
        assert( -1 != rc );

        sprintf(kvs_key, "job_mem_power.sample.%d",sample_id);
        sprintf(kvs_val, "%lf",mem_power_acc + _mem_value[0] + _mem_value[1]);
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

    double node_power; 
    double gpu_power; 
    double cpu_power; 
    double mem_power; 

    char my_hostname[256];
    gethostname(my_hostname, 256); 

    if( initialized <= 1000){
        initialized++;
	    flux_t *h = (flux_t*)arg;

        // Go off and take your measurement.  
        ret = variorum_get_node_power_json(power_obj);
        if (ret != 0)                                                                  
            flux_log(h, LOG_CRIT, "JSON: get node power failed!\n");           

        node_power = json_real_value(json_object_get(power_obj, "power_node_watts"));
        gpu_power = json_real_value(json_object_get(power_obj, "power_gpu_watts_socket_0")) + json_real_value(json_object_get(power_obj, "power_gpu_watts_socket_1")) ;
        cpu_power = json_real_value(json_object_get(power_obj, "power_cpu_watts_socket_0")) + json_real_value(json_object_get(power_obj, "power_cpu_watts_socket_1")) ;
        mem_power = json_real_value(json_object_get(power_obj, "power_mem_watts_socket_0")) + json_real_value(json_object_get(power_obj, "power_mem_watts_socket_1")) ;

        // Instantaneous power values at this sample, no accumulation yet. 
	    node_power_acc = node_power; 
	    gpu_power_acc = gpu_power; 
	    cpu_power_acc = cpu_power; 
	    mem_power_acc = mem_power; 

        // All ranks increment their sample id? 
        sample_id++; 

        flux_log(h, LOG_CRIT, "INFO: I am rank %d with node power %lf on sample %d and host %s\n", rank, node_power_acc, sample_id, my_hostname); 

	    // Then....
    	if( rank >= size/2 ){
		    // Just send the message.  These ranks don't do any combining.
	    	// flux_log(h, LOG_CRIT, "!!! %s:%d LEAF rank %d (size=%d) on host %s.\n", __FILE__, __LINE__, rank, size, hostname);
	    	flux_future_t *f = flux_rpc_pack (
		    	h, 				// flux_t *h
		    	"flux_pwr_mgr.collect_power", 			// char *topic
		    	dag[UPSTREAM],			// uint32_t nodeid (FLUX_NODEID_ANY, FLUX_NODEID_UPSTREAM, or a flux instance rank)
		    	FLUX_RPC_NORESPONSE,		// int flags (FLUX_RPC_NORESPONSE, FLUX_RPC_STREAMING, or NOFLAGS)
		    	"{s:i s:i s:s s:f s:f s:f s:f}", "sender", rank, "sample", sample_id, "hostname", my_hostname, 
                      "node_power", node_power_acc, "gpu_power", gpu_power_acc, "cpu_power", cpu_power_acc, "mem_power", mem_power_acc); 
		    	assert(f);
		    	flux_future_destroy(f);
        }
    }

        //Clean up JSON obj
        json_decref(power_obj); 
}



static const struct flux_msg_handler_spec htab[] = { 
    { FLUX_MSGTYPE_REQUEST,   "flux_pwr_mgr.collect_power",    		flux_pwr_mgr_collect_power_cb, 	0 },
	{ FLUX_MSGTYPE_REQUEST,   "flux_pwr_mgr.set_powercap",	flux_pwr_mgr_set_powercap_cb, 	0 },
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main (flux_t *h, int argc, char **argv){

	flux_get_rank(h, &rank);
	flux_get_size(h, &size);

	//flux_log(h, LOG_CRIT, "QQQ %s:%d Hello from rank %d of %d.\n", __FILE__, __LINE__, rank, size);

	// We don't have easy access to the topology of the underlying flux network, so we'll set up
	// an overlay instead.  
	dag[UPSTREAM]    = (rank==0)       ? -1 : rank / 2;
	dag[DOWNSTREAM1] = (rank >= size/2) ? -1 : rank * 2;
	dag[DOWNSTREAM2] = (rank >= size/2) ? -1 : rank * 2 + 1;
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

