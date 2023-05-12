
CPPFLAGS=-I/g/g90/patki1/src/variorum_install/include -I/g/g90/patki1/src/flux-framework/tioga-flux-install/include
LDFLAGS=-L/g/g90/patki1/src/variorum_install/lib -L/g/g90/patki1/src/flux-framework/tioga-flux-install/lib 

# Common
CFLAGS=-Wall -std=c11

all: flux_pwr_mgr.so

flux_pwr_mgr.so: flux_pwr_mgr.c
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(CFLAGS) -shared -D BUILD_MODULE -D_GNU_SOURCE -fPIC -o flux_pwr_mgr.so flux_pwr_mgr.c -lflux-core -lvariorum

clean:
	rm flux_pwr_mgr.so
