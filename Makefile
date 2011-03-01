PIN_HOME ?= ..
include $(PIN_HOME)/makefile.gnu.config
LINKER?=${CXX}
CXXFLAGS ?= -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT)

CXX=g++

all: mica

mica_all.o: mica.h mica_all.h mica_all.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_all.cpp -o mica_all.o

mica_init.o: mica.h mica_init.h mica_init.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_init.cpp -o mica_init.o

mica_utils.o: mica.h mica_utils.h mica_utils.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_utils.cpp -o mica_utils.o

mica_ilp.o: mica.h mica_ilp.h mica_ilp.cpp 
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_ilp.cpp -o mica_ilp.o

mica_itypes.o: mica.h mica_itypes.h mica_itypes.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_itypes.cpp -o mica_itypes.o

mica_ppm.o: mica.h mica_ppm.h mica_ppm.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_ppm.cpp -o mica_ppm.o

mica_reg.o: mica.h mica_reg.h mica_reg.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_reg.cpp -o mica_reg.o

mica_stride.o: mica.h mica_stride.h mica_stride.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_stride.cpp -o mica_stride.o

mica_memfootprint.o: mica.h mica_memfootprint.h mica_memfootprint.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_memfootprint.cpp -o mica_memfootprint.o

mica_memreusedist.o: mica.h mica_memreusedist.h mica_memreusedist.cpp
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica_memreusedist.cpp -o mica_memreusedist.o

mica.o: mica.h mica.cpp mica_init.h
	$(CXX) -g -c $(CXXFLAGS) $(PIN_CXXFLAGS) mica.cpp -o mica.o

mica: mica.h mica.o mica_all.o mica_init.o mica_utils.o mica_ilp.o mica_itypes.o mica_ppm.o mica_reg.o mica_stride.o mica_memfootprint.o mica_memreusedist.o
	$(CXX) -g $(PIN_LDFLAGS) $(LINK_DEBUG) mica.o mica_all.o mica_init.o mica_utils.o mica_ilp.o mica_itypes.o mica_ppm.o mica_reg.o mica_stride.o mica_memfootprint.o mica_memreusedist.o -o mica.so $(PIN_LPATHS) $(PIN_LIBS) $(DBG)


clean: 
	rm -f *.o mica.so *pin*out mica*log*
