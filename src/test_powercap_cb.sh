flux exec -r all flux /usr/bin/python3 -c 'import flux; h=flux.Flux(); print(h.rpc("flux_pwr_mgr.set_powercap", { "node": 200 }).get())'  
