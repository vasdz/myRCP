NAME = libmysyslog
SRC = libmysyslog.c
OBJ = ../../build/$(NAME).o
HDR = libmysyslog.h

all: ../../build $(OBJ)

../../build:
	mkdir -p ../../build

$(OBJ): $(SRC) $(HDR)
	gcc -c $(SRC) -o $(OBJ)

clean:
	rm -f $(OBJ)
	rm -rf build-deb

deb: all
	mkdir -p ../../deb
	mkdir -p build-deb/DEBIAN
	mkdir -p build-deb/usr/include/$(NAME)
	mkdir -p build-deb/usr/lib/$(NAME)
	cp $(HDR) build-deb/usr/include/$(NAME)/
	cp $(OBJ) build-deb/usr/lib/$(NAME)/
	echo "Package: $(NAME)" > build-deb/DEBIAN/control
	echo "Version: 1.0" >> build-deb/DEBIAN/control
	echo "Section: base" >> build-deb/DEBIAN/control
	echo "Priority: optional" >> build-deb/DEBIAN/control
	echo "Architecture: all" >> build-deb/DEBIAN/control
	echo "Maintainer: ttsaga <ttsaga @gmail.com>" >> build-deb/DEBIAN/control
	echo "Description: library" >> build-deb/DEBIAN/control
	chmod 0755 build-deb/DEBIAN
	chmod g-s build-deb/DEBIAN
	chmod 755 build-deb/DEBIAN
	chmod 0644 build-deb/DEBIAN/control
	fakeroot dpkg-deb --build build-deb ../../deb/libmysyslog_1.0_amd64.deb
