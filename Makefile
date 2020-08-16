# Solvent
#CPPFLAGS=-I/f/install/variorium/include -I/f/install/flux-core/include
#LDFLAGS=-L/f/install/variorum/lib -L/f/install/flux-core/lib

# Quartz-rountree
CPPFLAGS=-I/g/g24/rountree/FLUX/install/include
LDFLAGS=-L/g/g24/rountree/FLUX/install/lib

# Common
CFLAGS=-Wall -std=c11

all: reduce.so 

reduce.so: reduce.c
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(CFLAGS) -shared -D BUILD_MODULE -fPIC -o reduce.so reduce.c -lflux-core -lvariorum

clean:
	rm reduce.so 
