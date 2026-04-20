APP := insurance_app
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

# Если у тебя Qt5, запусти: make QT=Qt5Widgets
QT ?= Qt6Widgets
QT_CFLAGS := $(shell pkg-config --cflags $(QT))
QT_LIBS := $(shell pkg-config --libs $(QT))

.PHONY: all run clean

all: $(APP)

$(APP): main.cpp my_classes.cpp my_classes.h
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) main.cpp my_classes.cpp -o $@ $(QT_LIBS)
	@rm -f *.o

run: $(APP)
	./$(APP)

clean:
	rm -f $(APP) *.o
