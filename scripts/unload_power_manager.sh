#!/bin/bash

#Step 4: Unload the Flux power module

flux exec -r all flux module unload pwr_mgr
flux jobtap remove libjob_notification.so
