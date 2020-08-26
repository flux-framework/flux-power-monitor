flux /usr/bin/python3 -c 'import flux; h=flux.Flux(); print(h.rpc("reduce.set_powercap", { "node": 200 }).get())'  
#flux /usr/bin/python3 -c 'import flux; h=flux.Flux(); print(h.rpc("echo.echo", { "test": 42 }).get())'  
