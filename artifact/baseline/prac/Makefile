CXX := g++
CXXFLAGS := -march=native -std=c++17 -Wall -Wno-ignored-attributes -ggdb -O3
LDFLAGS := -ggdb
LDLIBS := -lbsd -lboost_system -lboost_context -lboost_chrono \
          -lboost_thread -lpthread

# Enable this to have all communication logged to stdout
# CXXFLAGS += -DVERBOSE_COMMS

OUT_DIR := out
BIN := $(OUT_DIR)/prac

SRCS := prac.cpp mpcio.cpp preproc.cpp online.cpp mpcops.cpp rdpf.cpp \
        cdpf.cpp duoram.cpp cell.cpp bst.cpp avl.cpp heap.cpp \
        heapsampler.cpp

OBJS := $(SRCS:%.cpp=$(OUT_DIR)/%.o)
ASMS := $(SRCS:%.cpp=$(OUT_DIR)/%.s)
DEPS := $(OBJS:.o=.d)

.PHONY: all reset clean depend

all: $(BIN)

$(BIN): $(OBJS) | $(OUT_DIR)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OUT_DIR)/%.o: %.cpp | $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(OUT_DIR)/%.s: %.cpp | $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -S -o $@ $<

$(OUT_DIR):
	mkdir -p $@

# Remove the files created by the preprocessing phase
reset:
	-rm -f *.p[012].t*

clean: reset
	-rm -rf $(OUT_DIR)

depend:
	@echo "Dependencies are generated automatically during compilation."

-include $(DEPS)
