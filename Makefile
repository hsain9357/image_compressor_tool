CXX = g++
CXXFLAGS = -std=c++17 -O3 -I./include
LDFLAGS = -ljpeg

TARGET = imgcomp
SRC = src/main.cpp
PREFIX = /usr/local

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) $(PREFIX)/bin/

uninstall:
	sudo rm -f $(PREFIX)/bin/$(TARGET)

.PHONY: all clean install uninstall
