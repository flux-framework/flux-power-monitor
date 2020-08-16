CPPFLAGS=-I/f/install/variorium/include -I/f/install/flux-core/include
LDFLAGS=-L/f/install/variorum/lib -L/f/install/flux-core/lib
CFLAGS=-Wall -std=c11

all: reduce.so 

reduce.so: 
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(CFLAGS) -shared -D BUILD_MODULE -fPIC -o reduce.so reduce.c -lflux-core -lvariorum

clean:
	rm reduce.so 
