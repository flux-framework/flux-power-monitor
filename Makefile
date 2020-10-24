# Solvent
#CPPFLAGS=-I/f/install/variorium/include -I/f/install/flux-core/include
#LDFLAGS=-L/f/install/variorum/lib -L/f/install/flux-core/lib


#CPPFLAGS=-I/g/g90/patki1/src/variorum_install_lassen/include -I/g/g90/patki1/src/flux-framework/flux-install/include
#LDFLAGS=-L/g/g90/patki1/src/variorum_install_lassen/lib -L/g/g90/patki1/src/flux-framework/flux-install/lib 

CPPFLAGS=-I/g/g90/patki1/src/variorum_install/include -I/g/g90/patki1/src/flux-framework/flux-install/include
LDFLAGS=-L/g/g90/patki1/src/variorum_install/lib -L/g/g90/patki1/src/flux-framework/flux-install/lib 

# Quartz-rountree
#CPPFLAGS=-I/g/g24/rountree/FLUX/install/include
#LDFLAGS=-L/g/g24/rountree/FLUX/install/lib

# Common
CFLAGS=-Wall -std=c11

all: flux_pwr_mgr.so

flux_pwr_mgr.so: flux_pwr_mgr.c
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(CFLAGS) -shared -D BUILD_MODULE -fPIC -o flux_pwr_mgr.so flux_pwr_mgr.c -lflux-core -lvariorum

clean:
	rm flux_pwr_mgr.so
