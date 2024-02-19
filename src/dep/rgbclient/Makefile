##
## RGBClient
## 2024, Akop Karapetyan
##

RGB_ROOT=../rpi-rgb-led-matrix
RGB_LIBDIR=$(RGB_ROOT)/lib

INCLUDE=-I$(RGB_ROOT)/include -Iinclude
LIB=-L$(RGB_LIBDIR)
DEFINE=
CFLAGS=-Wall -O2
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lrgbmatrix $(LIB)
EXE=rgbclient

all: $(EXE)

$(RGB_LIBDIR)/librgbmatrix.a:
	@echo Building RGB matrix libraries...
	@cd $(RGB_LIBDIR); $(MAKE)

rgbclient: rgbclient.o $(RGB_LIBDIR)/librgbmatrix.a
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(INCLUDE) $(DEFINE) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(DEFINE) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf *.o $(EXE)
	@cd $(RGB_LIBDIR); $(MAKE) clean
