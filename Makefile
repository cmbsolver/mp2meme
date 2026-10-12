-include config.mk

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall

TARGET = mp2meme
SRCS = src/main.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(TARGET)"

clean:
	rm -f $(OBJS) $(TARGET)

distclean: clean
	rm -f config.mk

.PHONY: all install uninstall clean distclean
