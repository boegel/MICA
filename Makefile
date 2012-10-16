PIN_HOME ?= ..
include $(PIN_HOME)/makefile.gnu.config
LINKER?=${CXX}
CXXFLAGS ?= -DVERBOSE -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT)
CXX=g++
SRCS = *.cpp

all: depend mica.so

%.o: %.cpp
	$(CXX) -g -o $@ -c $< $(CXXFLAGS) $(PIN_CXXFLAGS) 

mica.so: mica.o mica_all.o mica_init.o mica_utils.o mica_ilp.o mica_itypes.o mica_ppm.o mica_reg.o mica_stride.o mica_memfootprint.o mica_memstackdist.o
	$(CXX) -g -o $@ $(PIN_LDFLAGS) $(LINK_DEBUG) $^ $(PIN_LPATHS) $(PIN_LIBS) $(DBG)

clean:
	@rm -f *.o mica.so *pin*out mica*log* .depend

-include .depend

depend: $(SRCS)
	$(CXX) -MM $(CXXFLAGS) $(PIN_CXXFLAGS) $^ > .depend
