##
## RGBClient
## 2024, Akop Karapetyan
##

INCLUDE=-I../include
DEFINE=
CFLAGS=-Wall -O2
CXXFLAGS=$(CFLAGS)
SERVER=librgbserver

all: $(SERVER).a $(SERVER).so.1

$(SERVER).a: rgbserver.o
	$(AR) rcs $@ $^

$(SERVER).so.1: rgbserver.o
	$(CXX) -shared -Wl,-soname,$@ -o $@ $^

%.o: %.c
	$(CC) $(INCLUDE) $(DEFINE) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(DEFINE) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf *.o *.a *.so.1
