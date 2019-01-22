OBJECTS = Daemon.o
default: Daemon

%.o: %.c
	gcc -c $< -o $@

Daemon: $(OBJECTS)
	gcc $(OBJECTS) -o $@

clean:
	-rm -f $(OBJECTS)
	-rm -f Daemon
