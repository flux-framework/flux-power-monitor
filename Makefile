# Solvent
#CPPFLAGS=-I/f/install/variorium/include -I/f/install/flux-core/include
#LDFLAGS=-L/f/install/variorum/lib -L/f/install/flux-core/lib

CPPFLAGS=-I/g/g90/patki1/src/variorum_install/include                              
LDFLAGS=-L/g/g90/patki1/src/variorum_install/lib  

# Quartz-rountree
#CPPFLAGS=-I/g/g24/rountree/FLUX/install/include
#LDFLAGS=-L/g/g24/rountree/FLUX/install/lib

# Common
CFLAGS=-Wall -std=c11

all: reduce.so reduce_json.so

reduce.so: reduce.c
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(CFLAGS) -shared -D BUILD_MODULE -fPIC -o reduce.so reduce.c -lflux-core -lvariorum

reduce_json.so: reduce_json.c
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(CFLAGS) -shared -D BUILD_MODULE -fPIC -o reduce_json.so reduce_json.c -lflux-core -lvariorum

clean:
	rm reduce.so reduce_json.so
