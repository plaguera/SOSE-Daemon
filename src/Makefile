D_OBJECTS = Daemon.o Utils.o
S_OBJECTS = network/Server.o Utils.o
C_OBJECTS = network/Client.o Utils.o
default: Daemon

%.o: %.c
	gcc -c $< -o $@

Daemon: $(D_OBJECTS)
	gcc $(D_OBJECTS) -o $@

Server: $(S_OBJECTS)
	gcc $(S_OBJECTS) -o $@ -lpthread

Client: $(C_OBJECTS)
	gcc $(C_OBJECTS) -o $@ -lpthread

clean:
	-rm -f $(D_OBJECTS) $(S_OBJECTS) $(C_OBJECTS)
	-rm -f Daemon network/Server network/Client
	-rm -f ../log/*
