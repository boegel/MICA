ifdef PIN_ROOT
CONFIG_ROOT := $(PIN_ROOT)/source/tools/Config
else
CONFIG_ROOT := ../Config
endif

include $(CONFIG_ROOT)/makefile.config
include $(TOOLS_ROOT)/Config/makefile.default.rules
CXXFLAGS = -std=gnu++11 -DVERBOSE -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT) 

SRC_DIR := .
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJDIR)%$(OBJ_SUFFIX),$(SRC_FILES))

all: $(OBJDIR)mica$(PINTOOL_SUFFIX)

# Build the intermediate object file.
$(OBJDIR)%$(OBJ_SUFFIX): %.cpp
	$(CXX) $(CXXFLAGS) $(TOOL_CXXFLAGS_NOOPT) $(COMP_OBJ)$@ $< 

$(OBJDIR)mica$(PINTOOL_SUFFIX): $(OBJ_FILES)
	$(LINKER) $(TOOL_LDFLAGS) $(LINK_EXE)$@ $^ $(TOOL_LPATHS) $(TOOL_LIBS)

