# 'make' builds libHalide.a, the internal test suite, and runs the internal test suite
# 'make tests' builds and runs all the end-to-end tests in the test subdirectory
# 'make test_foo' builds and runs test/foo.cpp for any cpp file in the test folder
# 'make test_apps' checks some of the apps build and run (but does not check their output)

# Set RUNDEBUG to select debugging options for running test programs
RUNDEBUG = HL_DEBUG_LOGFILE=0 HL_NUMTHREADS=4 HL_DEBUG_CODEGEN=0 MALLOC_TRACE=$@.trace

CXX ?= g++ 
LLVM_CONFIG ?= llvm-config
LLVM_COMPONENTS= $(shell $(LLVM_CONFIG) --components)
LLVM_VERSION = $(shell $(LLVM_CONFIG) --version)
CLANG ?= clang
CLANG_VERSION = $(shell $(CLANG) --version)
LLVM_BINDIR = $(shell $(LLVM_CONFIG) --bindir)
LLVM_LIBDIR = $(shell $(LLVM_CONFIG) --libdir)
LLVM_AS = $(LLVM_BINDIR)/llvm-as
LLVM_CXX_FLAGS = $(shell $(LLVM_CONFIG) --cppflags)

# llvm_config doesn't always point to the correct include
# directory. We haven't figured out why yet.
LLVM_CXX_FLAGS += -I$(shell $(LLVM_CONFIG) --src-root)/include

WITH_NATIVE_CLIENT ?= $(findstring nacltransforms, $(LLVM_COMPONENTS))
NATIVE_CLIENT_CXX_FLAGS = $(if $(WITH_NATIVE_CLIENT), "-DWITH_NATIVE_CLIENT=1", )
NATIVE_CLIENT_ARCHS = $(if $(WITH_NATIVE_CLIENT), x86_nacl x86_32_nacl arm_nacl, )
NATIVE_CLIENT_LLVM_CONFIG_LIB = $(if $(WITH_NATIVE_CLIENT), nacltransforms, )
NATIVE_CLIENT_ROOT ?=

WITH_PTX ?= $(findstring nvptx, $(LLVM_COMPONENTS))
PTX_CXX_FLAGS=$(if $(WITH_PTX), "-DWITH_PTX=1", )
PTX_ARCHS=$(if $(WITH_PTX), ptx_host ptx_dev, )
PTX_LLVM_CONFIG_LIB=$(if $(WITH_PTX), nvptx, )

CXX_FLAGS = -Wall -Werror -fno-rtti -Woverloaded-virtual -Wno-unused-function -fPIC -O3 
CXX_FLAGS += $(LLVM_CXX_FLAGS)
CXX_FLAGS += $(NATIVE_CLIENT_CXX_FLAGS)
CXX_FLAGS += $(PTX_CXX_FLAGS)
LIBS = -L $(LLVM_LIBDIR) $(shell $(LLVM_CONFIG) --libs bitwriter bitreader x86 arm linker ipo mcjit jit $(NATIVE_CLIENT_LLVM_CONFIG_LIB) $(PTX_LLVM_CONFIG_LIB))

TEST_CXX_FLAGS ?=
UNAME = $(shell uname)
ifeq ($(UNAME), Linux)
TEST_CXX_FLAGS += -rdynamic
endif

ifdef BUILD_PREFIX
BUILD_DIR = build/$(BUILD_PREFIX)
BIN_DIR = bin/$(BUILD_PREFIX)
DISTRIB_DIR=distrib/$(BUILD_PREFIX)
# LH: Add library directory to build library for static linking
LIB_DIR=lib/$(BUILD_PREFIX)
else
BUILD_DIR = build
BIN_DIR = bin
DISTRIB_DIR=distrib
# LH: Add library directory to build library for static linking
LIB_DIR = lib
endif

SOURCE_FILES = CodeGen.cpp CodeGen_Internal.cpp CodeGen_X86.cpp CodeGen_GPU_Host.cpp CodeGen_PTX_Dev.cpp CodeGen_GPU_Dev.cpp CodeGen_Posix.cpp CodeGen_ARM.cpp IR.cpp IRMutator.cpp IRPrinter.cpp IRVisitor.cpp CodeGen_C.cpp Substitute.cpp ModulusRemainder.cpp Bounds.cpp Derivative.cpp Func.cpp Simplify.cpp IREquality.cpp Util.cpp Function.cpp IROperator.cpp Lower.cpp Log.cpp Parameter.cpp Reduction.cpp RDom.cpp Tracing.cpp StorageFlattening.cpp VectorizeLoops.cpp UnrollLoops.cpp BoundsInference.cpp IRMatch.cpp StmtCompiler.cpp integer_division_table.cpp SlidingWindow.cpp StorageFolding.cpp InlineReductions.cpp RemoveTrivialForLoops.cpp Deinterleave.cpp DebugToFile.cpp Type.cpp JITCompiledModule.cpp EarlyFree.cpp UniquifyVariableNames.cpp

# The externally-visible header files that go into making Halide.h. Don't include anything here that includes llvm headers.
HEADER_FILES = Util.h Type.h Argument.h Bounds.h BoundsInference.h Buffer.h buffer_t.h CodeGen_C.h CodeGen.h CodeGen_X86.h CodeGen_GPU_Host.h CodeGen_PTX_Dev.h CodeGen_GPU_Dev.h Deinterleave.h Derivative.h Extern.h Func.h Function.h Image.h InlineReductions.h integer_division_table.h IntrusivePtr.h IREquality.h IR.h IRMatch.h IRMutator.h IROperator.h IRPrinter.h IRVisitor.h JITCompiledModule.h Lambda.h Log.h Lower.h MainPage.h ModulusRemainder.h Parameter.h Param.h RDom.h Reduction.h RemoveTrivialForLoops.h Schedule.h Scope.h Simplify.h SlidingWindow.h StmtCompiler.h StorageFlattening.h StorageFolding.h Substitute.h Tracing.h UnrollLoops.h Var.h VectorizeLoops.h CodeGen_Posix.h CodeGen_ARM.h DebugToFile.h EarlyFree.h UniquifyVariableNames.h

SOURCES = $(SOURCE_FILES:%.cpp=src/%.cpp)
OBJECTS = $(SOURCE_FILES:%.cpp=$(BUILD_DIR)/%.o)
HEADERS = $(HEADER_FILES:%.h=src/%.h)

STDLIB_ARCHS = x86 x86_avx x86_32 arm arm_android $(PTX_ARCHS) $(NATIVE_CLIENT_ARCHS)

#LH: Extended source files are listed separately so that Git merge is easier.
# LHHACK: RemoveDeadLets is reinstated as a hack to get Lower.cpp working for now.
LH_SOURCE_FILES = Border.cpp Clamp.cpp DomainInference.cpp LowerClamp.cpp LoopPartition.cpp Options.cpp Interval.cpp InlineLet.cpp IRCacheMutator.cpp Statistics.cpp CodeLogger.cpp IRLazyScope.cpp Context.cpp IRProcess.cpp DomInterval.cpp BoundsSimplify.cpp IntRange.cpp RemoveDeadLets.cpp
LH_HEADER_FILES = Border.h Clamp.h DomainInference.h LowerClamp.h LoopPartition.h Options.h Interval.h Solver.h InlineLet.h IRCacheMutator.h Statistics.h CodeLogger.h IRLazyScope.h Context.h IRProcess.h DomInterval.h BoundsAnalysis.h BoundsSimplify.h IntRange.h RemoveDeadLets.h
SOURCES = $(LH_SOURCE_FILES:%.cpp=src/%.cpp) $(SOURCE_FILES:%.cpp=src/%.cpp)
OBJECTS = $(LH_SOURCE_FILES:%.cpp=$(BUILD_DIR)/%.o) $(SOURCE_FILES:%.cpp=$(BUILD_DIR)/%.o)
GOBJECTS = $(LH_SOURCE_FILES:%.cpp=build_g/%.o) $(SOURCE_FILES:%.cpp=build_g/%.o)
HEADERS = $(HEADER_FILES:%.h=src/%.h) $(LH_HEADER_FILES:%.h=src/%.h)
# LH: Rename the library so that comparisons can be performed
NAME = HalideStar


INITIAL_MODULES = $(STDLIB_ARCHS:%=$(BUILD_DIR)/initmod.%.o)

.PHONY: all
all: $(BIN_DIR)/lib$(NAME).a $(BIN_DIR)/lib$(NAME).so include/Halide.h test_internal

$(BIN_DIR)/lib$(NAME).a: $(OBJECTS) $(INITIAL_MODULES)
	@-mkdir -p $(BIN_DIR)
	ld -r -o $(BUILD_DIR)/Halide.o $(OBJECTS) $(INITIAL_MODULES) $(LIBS)
	rm -f $(BIN_DIR)/lib$(NAME).a
	ar q $(BIN_DIR)/lib$(NAME).a $(BUILD_DIR)/Halide.o
	ranlib $(BIN_DIR)/lib$(NAME).a

$(BIN_DIR)/lib$(NAME).so: $(BIN_DIR)/lib$(NAME).a
	$(CXX) -shared $(OBJECTS) $(INITIAL_MODULES) $(LIBS) -o $(BIN_DIR)/lib$(NAME).so

# LH: Debug version of the library for static linking
debug: $(LIB_DIR)/libg$(NAME).a include/Halide.h gtest_internal

$(LIB_DIR)/libg$(NAME).a: $(GOBJECTS) $(INITIAL_MODULES)
	@-mkdir -p $(LIB_DIR)
	ld -r -o build_g/Halide.o $(GOBJECTS) $(INITIAL_MODULES) $(LIBS)
	rm -f $(LIB_DIR)/libg$(NAME).a
	ar q $(LIB_DIR)/libg$(NAME).a build_g/Halide.o
	ranlib $(LIB_DIR)/libg$(NAME).a
    
$(LIB_DIR)/lib$(NAME).a: $(BIN_DIR)/lib$(NAME).a
	@-mkdir -p $(LIB_DIR)
	cp $(BIN_DIR)/lib$(NAME).a $(LIB_DIR)/lib$(NAME).a

include/Halide.h: $(HEADERS) $(BIN_DIR)/build_halide_h
	mkdir -p include
	cd src; ../$(BIN_DIR)/build_halide_h $(HEADER_FILES) $(LH_HEADER_FILES) > ../include/Halide.h; cd ..

$(BIN_DIR)/build_halide_h: src/build_halide_h.cpp
	g++ $< -o $@

RUNTIME_OPTS_x86 = -march=corei7 
RUNTIME_OPTS_x86_avx = -march=corei7-avx 
RUNTIME_OPTS_x86_32 = -m32 -march=atom
RUNTIME_OPTS_arm = -m32 
RUNTIME_OPTS_arm_android = -m32 
RUNTIME_OPTS_ptx_host = $(RUNTIME_OPTS_x86) 
RUNTIME_OPTS_ptx_dev = 
RUNTIME_OPTS_x86_nacl = -Xclang -triple -Xclang x86_64-unknown-nacl -m64 -march=corei7 -isystem $(NATIVE_CLIENT_X86_INCLUDE)
RUNTIME_OPTS_x86_32_nacl = -Xclang -triple -Xclang i386-unknown-nacl -m32 -march=atom -isystem $(NATIVE_CLIENT_X86_INCLUDE)
RUNTIME_OPTS_arm_nacl = -Xclang -target-cpu -Xclang "" -Xclang -triple -Xclang arm-unknown-nacl -m32 -isystem $(NATIVE_CLIENT_ARM_INCLUDE)
RUNTIME_LL_STUBS_x86 = src/runtime/x86.ll src/runtime/x86_sse41.ll
RUNTIME_LL_STUBS_x86_32 = src/runtime/x86.ll
RUNTIME_LL_STUBS_x86_avx = src/runtime/x86.ll src/runtime/x86_sse41.ll src/runtime/x86_avx.ll
RUNTIME_LL_STUBS_arm = src/runtime/arm.ll
RUNTIME_LL_STUBS_arm_android = src/runtime/arm.ll
RUNTIME_LL_STUBS_ptx_host = $(RUNTIME_LL_STUBS_x86)
RUNTIME_LL_STUBS_ptx_dev = src/runtime/ptx_dev.ll
RUNTIME_LL_STUBS_x86_nacl = src/runtime/x86.ll src/runtime/x86_sse41.ll
RUNTIME_LL_STUBS_x86_32_nacl = src/runtime/x86.ll
RUNTIME_LL_STUBS_arm_nacl = src/runtime/arm.ll

# Include the generated make rules
-include $(OBJECTS:.o=.d)

# LH: Include generated make rules for the debugging compilation
-include $(GOBJECTS:.o=.d)

$(BUILD_DIR)/initmod.%.cpp: $(BIN_DIR)/bitcode2cpp src/runtime/*.cpp src/runtime/*.ll $(BUILD_DIR)/llvm_ok $(BUILD_DIR)/clang_ok
	@-mkdir -p $(BUILD_DIR)
	$(CLANG) $(RUNTIME_OPTS_$*) -emit-llvm -O3 -S src/runtime/runtime.$*.cpp -o - | \
	cat - $(RUNTIME_LL_STUBS_$*) | \
	$(LLVM_AS) -o - | \
	./$(BIN_DIR)/bitcode2cpp $* > $@

$(BIN_DIR)/bitcode2cpp: src/bitcode2cpp.cpp
	@-mkdir -p $(BIN_DIR)
	$(CXX) $< -o $@

$(BUILD_DIR)/initmod.%.o: $(BUILD_DIR)/initmod.%.cpp
	$(CXX) -c $< -o $@ -MMD -MP -MF $(BUILD_DIR)/$*.d -MT $(BUILD_DIR)/$*.o

$(BUILD_DIR)/%.o: src/%.cpp src/%.h $(BUILD_DIR)/llvm_ok
	@-mkdir -p $(BUILD_DIR)
	$(CXX) $(CXX_FLAGS) -c $< -o $@ -MMD -MP -MF $(BUILD_DIR)/$*.d -MT $(BUILD_DIR)/$*.o 
# -MF: Specify the file for the generated make rules.
# -MT: Specify the target for the generated make rules.

# LH: build_g builds the debugging library (-g option)
build_g/%.o: src/%.cpp src/%.h build/llvm_ok
	@-mkdir -p build_g
	$(CXX) $(CXX_FLAGS) -g -c $< -o $@ -MMD -MP -MF build_g/$*.d -MT build_g/$*.o 

.PHONY: clean
clean:
	rm -rf $(BIN_DIR)/*
	rm -rf $(BUILD_DIR)/*
	rm -rf $(DISTRIB_DIR)/*
	rm -rf include/*
	rm -rf doc
	rm -rf $(LIB_DIR)/*
	rm -rf build_g/*

.SECONDARY:

TESTS = $(shell ls test/*.cpp)
ERROR_TESTS = $(shell ls test/error/*.cpp)

# TODO: move this implementation into Makefile.tests which contains a .NOTPARALLEL rule?
tests: build_tests run_tests

# LH: Add jit speed test to the list of tests to be run
tests: test_jit_speed_fast

run_tests: $(TESTS:test/%.cpp=test_%) $(ERROR_TESTS:test/error/%.cpp=error_%)
build_tests: $(TESTS:test/%.cpp=$(BIN_DIR)/test_%) $(ERROR_TESTS:test/error/%.cpp=$(BIN_DIR)/error_%)

$(BIN_DIR)/test_internal: test/internal.cpp $(BIN_DIR)/lib$(NAME).so
	$(CXX) $(CXX_FLAGS)  $< -Isrc -L$(BIN_DIR) -l$(NAME) -lpthread -ldl -o $@	

$(BIN_DIR)/test_%: test/%.cpp $(BIN_DIR)/lib$(NAME).so include/Halide.h
	$(CXX) $(TEST_CXX_FLAGS) -O3 $<  -Iinclude -L$(BIN_DIR) -l$(NAME) -lpthread -ldl -o $@	

$(BIN_DIR)/error_%: test/error/%.cpp $(BIN_DIR)/lib$(NAME).so include/Halide.h
	$(CXX) $(TEST_CXX_FLAGS) -O3 $<  -Iinclude -L$(BIN_DIR) -l$(NAME) -lpthread -ldl -o $@	

# LH: Add RUNDEBUG options and specify log file name for debug output
test_%: $(BIN_DIR)/test_%
	@-mkdir -p tmp
	cd tmp ; $(RUNDEBUG) HL_LOG_NAME=$@ DYLD_LIBRARY_PATH=../$(BIN_DIR) LD_LIBRARY_PATH=../$(BIN_DIR) ../$<
	@-echo

error_%: $(BIN_DIR)/error_%
	@-mkdir -p tmp
	cd tmp ; DYLD_LIBRARY_PATH=../$(BIN_DIR) LD_LIBRARY_PATH=../$(BIN_DIR) ../$< 2>&1 | egrep --q "Assertion.*failed"
	@-echo

# LH: My jit speed test.
$(BIN_DIR)/test_jit_speed: test/jit_speed.cpp $(BIN_DIR)/lib$(NAME).so include/Halide.h
	$(CXX) $(TEST_CXX_FLAGS) -O3 $<  -DHALIDE_NEW=1 -DHALIDE_VERSION=999999 -Iinclude -L$(BIN_DIR) -l$(NAME) -lpthread -ldl -o $@	

#LH: Compilation with debugging option and static link of debugging library
gtests: $(TESTS:test/%.cpp=gtest_%)

$(BIN_DIR)/gtest_internal: test/internal.cpp $(LIB_DIR)/libg$(NAME).a
	$(CXX) -g $< -Isrc -L$(LIB_DIR) -lg$(NAME) -lpthread -ldl -o $@	

$(BIN_DIR)/gtest_%: test/%.cpp $(LIB_DIR)/libg$(NAME).a include/Halide.h
	$(CXX) -g $(TEST_CXX_FLAGS) $<  -Iinclude -L$(LIB_DIR) -lg$(NAME) -lpthread -ldl -o $@	

# LH: Run the compiler stress test without debugging options RUNDEBUG    
test_jit_stress: $(BIN_DIR)/test_jit_stress
	@-mkdir -p tmp
	cd tmp ; HL_LOG_NAME=$@ DYLD_LIBRARY_PATH=../$(BIN_DIR) LD_LIBRARY_PATH=../$(BIN_DIR) ../$<
	@-echo

# LH: Run the fast jit speed test without RUNDEBUG options
test_jit_speed_fast: $(BIN_DIR)/test_jit_speed
	@-mkdir -p tmp
	cd tmp ; HL_LOG_NAME=$@ DYLD_LIBRARY_PATH=../$(BIN_DIR) LD_LIBRARY_PATH=../$(BIN_DIR) ../$<
	@-echo

test_jit_speed: $(BIN_DIR)/test_jit_speed
	@-mkdir -p tmp
	cd tmp ; $(RUNDEBUG) HL_LOG_NAME=$@ DYLD_LIBRARY_PATH=../$(BIN_DIR) LD_LIBRARY_PATH=../$(BIN_DIR) ../$< -log 5
	@-echo

# LH: gdbtest_... is a target that starts gdb on a test program
gdbtest_%: $(BIN_DIR)/test_%
	@-mkdir -p tmp
	cd tmp ; $(RUNDEBUG) HL_LOG_NAME=$@ DYLD_LIBRARY_PATH=../$(BIN_DIR) LD_LIBRARY_PATH=../$(BIN_DIR) gdb ../$<
	@-echo

gdbgtest_%: $(BIN_DIR)/gtest_%
	@-mkdir -p tmp
	cd tmp ; $(RUNDEBUG) HL_LOG_NAME=$@ DYLD_LIBRARY_PATH=../$(BIN_DIR) LD_LIBRARY_PATH=../$(BIN_DIR) gdb ../$<
	@-echo

# LH: gtest_... starts the -g compiled version of the specific test
gtest_%: $(BIN_DIR)/gtest_%
	@-mkdir -p tmp
	cd tmp ; $(RUNDEBUG) HL_LOG_NAME=$@ DYLD_LIBRARY_PATH=../$(BIN_DIR) LD_LIBRARY_PATH=../$(BIN_DIR) ../$<
	@-echo

.PHONY: test_apps
test_apps: $(BIN_DIR)/lib$(NAME).a
	make -C apps/bilateral_grid clean
	make -C apps/bilateral_grid out.png
	make -C apps/local_laplacian clean
	make -C apps/local_laplacian out.png
	make -C apps/interpolate clean
	make -C apps/interpolate out.png
	make -C apps/blur clean
	make -C apps/blur test
	./apps/blur/test
	make -C apps/wavelet filter
	cd apps/wavelet; ./filter input.png; cd ../..


ifneq (,$(findstring version 3.,$(CLANG_VERSION)))
ifeq (,$(findstring version 3.0,$(CLANG_VERSION)))
CLANG_OK=yes
endif
endif

ifneq (,$(findstring Apple clang version 4.0,$(CLANG_VERSION)))
CLANG_OK=yes
endif

ifneq (,$(findstring 3.,$(LLVM_VERSION)))
ifeq (,$(findstring 3.0,$(LLVM_VERSION)))
ifeq (,$(findstring 3.1,$(LLVM_VERSION)))
LLVM_OK=yes
endif
endif
endif

ifdef CLANG_OK
$(BUILD_DIR)/clang_ok:
	@echo "Found a new enough version of clang"
	mkdir -p $(BUILD_DIR)
	touch $(BUILD_DIR)/clang_ok
else
$(BUILD_DIR)/clang_ok:
	@echo "Can't find clang or version of clang too old (we need 3.1 or greater):"
	@echo "You can override this check by setting CLANG_OK=y"
	echo '$(CLANG_VERSION)'
	echo $(findstring version 3,$(CLANG_VERSION))
	echo $(findstring version 3.0,$(CLANG_VERSION))
	$(CLANG) --version
	@exit 1
endif

ifdef LLVM_OK
$(BUILD_DIR)/llvm_ok:
	@echo "Found a new enough version of llvm"
	mkdir -p $(BUILD_DIR)
	touch $(BUILD_DIR)/llvm_ok
else
$(BUILD_DIR)/llvm_ok:
	@echo "Can't find llvm or version of llvm too old (we need 3.2 or greater):"
	@echo "You can override this check by setting LLVM_OK=y"
	$(LLVM_CONFIG) --version
	@exit 1
endif

.PHONY: doc
docs: doc
doc: src test
	doxygen

$(DISTRIB_DIR)/halide.tgz: all
	mkdir -p $(DISTRIB_DIR)/include $(DISTRIB_DIR)/lib
	cp $(BIN_DIR)/libHalide.a $(BIN_DIR)/libHalide.so $(DISTRIB_DIR)/lib
	cp include/Halide.h $(DISTRIB_DIR)/include
	tar -czf $(DISTRIB_DIR)/halide.tgz -C $(DISTRIB_DIR) lib include

distrib: $(DISTRIB_DIR)/halide.tgz
