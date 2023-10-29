#!/bin/bash

#Step 4: Unload the Flux power module

flux exec -r all flux module unload flux_pwr_manager
flux jobtap remove libjob_notification.so
