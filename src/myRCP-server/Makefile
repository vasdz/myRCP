CC = gcc
CFLAGS = -Wall -I../../source/libmysyslog
LDFLAGS = -ljson-c
NAME = myRPC-server
SRC = myRPC-server.c
OBJ = ../../build/myRPC-server.o
SYSLOG_OBJ = ../../build/libmysyslog.o
BIN = ../../bin/myRPC-server

all: $(BIN)

$(OBJ): $(SRC)
	mkdir -p ../../build
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

$(BIN): $(OBJ) $(SYSLOG_OBJ)
	$(CC) $(OBJ) $(SYSLOG_OBJ) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(BIN)
	rm -rf build-deb

deb: all
	mkdir -p build-deb/usr/local/bin
	mkdir -p build-deb/etc/myRPC
	mkdir -p build-deb/lib/systemd/system
	mkdir -p build-deb/DEBIAN
	cp $(BIN) build-deb/usr/local/bin/
	cp myRPC.conf build-deb/etc/myRPC/
	cp users.conf build-deb/etc/myRPC/
	cp myRPC-server.service build-deb/lib/systemd/system/
	echo "Package: $(NAME)" > build-deb/DEBIAN/control
	echo "Version: 1.0" >> build-deb/DEBIAN/control
	echo "Section: base" >> build-deb/DEBIAN/control
	echo "Priority: optional" >> build-deb/DEBIAN/control
	echo "Architecture: amd64" >> build-deb/DEBIAN/control
	echo "Maintainer: ttsaga <ttsaga@gmail.com>" >> build-deb/DEBIAN/control
	echo "Description: myRPC-server" >> build-deb/DEBIAN/control
	chmod 0755 build-deb/DEBIAN
	chmod g-s build-deb/DEBIAN
	chmod 644 build-deb/DEBIAN/control
	mkdir -p ../../deb
	fakeroot dpkg-deb --build build-deb ../../deb/$(NAME)_1.0_amd64.deb
