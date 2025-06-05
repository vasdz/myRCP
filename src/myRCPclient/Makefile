NAME = myRPC-client
SRC = myRPC-client.c
OBJ = ../../build/$(NAME).o
MYSYSLOG_OBJ = ../../build/libmysyslog.o
BIN = ../../bin/$(NAME)
CC = gcc
CFLAGS = -Wall -I../../src/mysyslog
LDFLAGS = -ljson-c

all: ../../bin ../../build $(BIN)

../../bin:
	mkdir -p ../../bin

../../build:
	mkdir -p ../../build

../../build/libmysyslog.o: ../../source/libmysyslog/libmysyslog.c ../../source/libmysyslog/libmysyslog.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

$(BIN): $(OBJ) $(MYSYSLOG_OBJ)
	$(CC) $(OBJ) $(MYSYSLOG_OBJ) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(MYSYSLOG_OBJ) $(BIN)
	rm -rf build-deb

deb: all
	mkdir -p build-deb/usr/local/bin
	cp $(BIN) build-deb/usr/local/bin/
	mkdir -p build-deb/DEBIAN
	echo "Package: $(NAME)" > build-deb/DEBIAN/control
	echo "Version: 1.0" >> build-deb/DEBIAN/control
	echo "Section: base" >> build-deb/DEBIAN/control
	echo "Priority: optional" >> build-deb/DEBIAN/control
	echo "Architecture: amd64" >> build-deb/DEBIAN/control
	echo "Maintainer: ttsaga <p3edfr213231@gmail.com>" >> build-deb/DEBIAN/control
	echo "Description: myRCP-client program" >> build-deb/DEBIAN/control
	chmod 0755 build-deb/DEBIAN
	chmod 644 build-deb/DEBIAN/control
	mkdir -p ../../deb
	fakeroot dpkg-deb --build build-deb ../../deb/$(NAME)_1.0_amd64.deb
