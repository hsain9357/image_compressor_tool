# These vars are for OUR link/compile steps only. GNU make would otherwise
# export them into the third-party ./configure / cmake invocations below
# (zlib's configure reads $LDFLAGS verbatim and bakes it into libz.a's link
# rules, which breaks example/minigzip with "library 'imagequant_sys' not
# found"). Keep them out of the child environment.
unexport LDFLAGS CXXFLAGS CPPFLAGS

CXX = g++
CXXFLAGS = -std=c++17 -O3 -I./include -I./libimagequant/imagequant-sys
LDFLAGS = -L./libimagequant/target/release -limagequant_sys -lpthread -ldl

TARGET = imgcomp
SRC = src/main.cpp
PREFIX = /usr/local

STATIC_DIR ?= $(CURDIR)/.static-deps
# macOS ships no static libSystem, so fully-static linking (`-static`) is
# impossible there ("library 'crt0.o' not found"). Keep true static linking on
# Linux and fall back to a normal link on Darwin.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  STATIC_FLAGS =
  STATIC_LIBS  = -L$(STATIC_DIR)/lib -ljpeg -lwebp -lsharpyuv -lpng -lz -lm
else
  STATIC_FLAGS = -static -static-libgcc -static-libstdc++
  STATIC_LIBS  = -L$(STATIC_DIR)/lib -L$(STATIC_DIR)/lib64 -ljpeg -lwebp -lsharpyuv -lpng -lz -lm
endif

all: libimagequant/target/release/libimagequant_sys.a $(STATIC_DIR)/.stamp
	$(CXX) $(STATIC_FLAGS) $(CXXFLAGS) -I$(STATIC_DIR)/include $(SRC) -o $(TARGET) $(LDFLAGS) $(STATIC_LIBS)

libimagequant/target/release/libimagequant_sys.a:
	cargo build --release --manifest-path libimagequant/imagequant-sys/Cargo.toml

$(STATIC_DIR)/.stamp:
	mkdir -p $(STATIC_DIR)/src
	curl -sL https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.1.3.tar.gz -o $(STATIC_DIR)/src/jpeg.tar.gz
	curl -sL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz -o $(STATIC_DIR)/src/zlib.tar.gz
	curl -sL https://downloads.sourceforge.net/project/libpng/libpng16/1.6.53/libpng-1.6.53.tar.gz -o $(STATIC_DIR)/src/png.tar.gz
	curl -sL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz -o $(STATIC_DIR)/src/webp.tar.gz
	cd $(STATIC_DIR)/src && tar xzf zlib.tar.gz && cd zlib-1.3.1 && \
		LDFLAGS= LDSHAREDFLAGS= ./configure --static --prefix=$(STATIC_DIR) && \
		make -j$$(nproc) && make install
	cd $(STATIC_DIR)/src && tar xzf jpeg.tar.gz && cd libjpeg-turbo-3.1.3 && \
		env -u LDFLAGS cmake -DCMAKE_INSTALL_PREFIX=$(STATIC_DIR) -DCMAKE_EXE_LINKER_FLAGS= -DCMAKE_SHARED_LINKER_FLAGS= -DCMAKE_MODULE_LINKER_FLAGS= -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -B build && \
		env -u LDFLAGS cmake --build build -j$$(nproc) && cmake --install build
	cd $(STATIC_DIR)/src && tar xzf png.tar.gz && cd libpng-1.6.53 && \
		LDFLAGS= ./configure --disable-shared --enable-static --prefix=$(STATIC_DIR) && \
		make -j$$(nproc) && make install
	cd $(STATIC_DIR)/src && tar xzf webp.tar.gz && cd libwebp-1.6.0 && \
		env -u LDFLAGS cmake -DCMAKE_INSTALL_PREFIX=$(STATIC_DIR) -DCMAKE_EXE_LINKER_FLAGS= -DCMAKE_SHARED_LINKER_FLAGS= -DCMAKE_MODULE_LINKER_FLAGS= -DSHARED=OFF -DSTATIC=ON -B build && \
		env -u LDFLAGS cmake --build build -j$$(nproc) && cmake --install build
	touch $(STATIC_DIR)/.stamp

clean:
	rm -f $(TARGET)

distclean:
	rm -rf $(TARGET) $(STATIC_DIR)

install: $(TARGET)
	sudo cp $(TARGET) $(PREFIX)/bin/

uninstall:
	sudo rm -f $(PREFIX)/bin/$(TARGET)

.PHONY: all clean distclean install uninstall
