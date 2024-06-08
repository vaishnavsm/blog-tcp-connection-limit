
all: listener connector

out:
	-mkdir out

connector: out connector.c utils.c
	gcc connector.c utils.c -o out/connector

listener: out listener.c utils.c
	gcc listener.c utils.c -o out/listener
