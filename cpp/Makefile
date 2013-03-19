# 'make' builds libHalide.a, the internal test suite, and runs the internal test suite
# 'make tests' builds and runs all the end-to-end tests in the test subdirectory
# 'make test_foo' builds and runs test/foo.cpp for any cpp file in the test folder
# 'make test_apps' checks some of the apps build and run (but does not check their output)

CXX ?= g++
LLVM_CONFIG ?= llvm-config
LLVM_VERSION = $(shell $(LLVM_CONFIG) --version)
CLANG ?= clang
CLANG_VERSION = $(shell $(CLANG) --version)
LLVM_BINDIR = $(shell $(LLVM_CONFIG) --bindir)
LLVM_AS = $(LLVM_BINDIR)/llvm-as
CXX_FLAGS = $(shell $(LLVM_CONFIG) --cflags) -Wall -Werror -fno-rtti -Woverloaded-virtual -Wno-unused-function -Os
LIBS = -L $(shell $(LLVM_CONFIG) --libdir) $(shell $(LLVM_CONFIG) --libs jit bitwriter bitreader x86 arm linker nvptx ipo)

SOURCE_FILES = CodeGen.cpp CodeGen_X86.cpp CodeGen_Posix.cpp CodeGen_ARM.cpp IR.cpp IRMutator.cpp IRPrinter.cpp IRVisitor.cpp CodeGen_C.cpp Substitute.cpp ModulusRemainder.cpp Bounds.cpp Derivative.cpp Func.cpp Simplify.cpp IREquality.cpp Util.cpp Function.cpp IROperator.cpp Lower.cpp Log.cpp Parameter.cpp Reduction.cpp RDom.cpp Tracing.cpp RemoveDeadLets.cpp StorageFlattening.cpp VectorizeLoops.cpp UnrollLoops.cpp BoundsInference.cpp IRMatch.cpp StmtCompiler.cpp integer_division_table.cpp SlidingWindow.cpp StorageFolding.cpp InlineReductions.cpp RemoveTrivialForLoops.cpp Deinterleave.cpp DebugToFile.cpp

HEADER_FILES = Util.h Type.h Argument.h Bounds.h BoundsInference.h Buffer.h buffer_t.h CodeGen_C.h CodeGen.h CodeGen_X86.h Deinterleave.h Derivative.h Extern.h Func.h Function.h Image.h InlineReductions.h integer_division_table.h IntrusivePtr.h IREquality.h IR.h IRMatch.h IRMutator.h IROperator.h IRPrinter.h IRVisitor.h JITCompiledModule.h Lambda.h Log.h Lower.h MainPage.h ModulusRemainder.h Parameter.h Param.h RDom.h Reduction.h RemoveDeadLets.h RemoveTrivialForLoops.h Schedule.h Scope.h Simplify.h SlidingWindow.h StmtCompiler.h StorageFlattening.h StorageFolding.h Substitute.h Tracing.h UnrollLoops.h Var.h VectorizeLoops.h CodeGen_Posix.h CodeGen_ARM.h DebugToFile.h

#LH: List extensions separately to support automatic merging.
LH_SOURCE_FILES = Border.cpp Clamp.cpp DomainInference.cpp LowerClamp.cpp
LH_HEADER_FILES = Border.h Clamp.h DomainInference.h LowerClamp.h

SOURCES = $(LH_SOURCE_FILES:%.cpp=src/%.cpp) $(SOURCE_FILES:%.cpp=src/%.cpp)
OBJECTS = $(LH_SOURCE_FILES:%.cpp=build/%.o) $(SOURCE_FILES:%.cpp=build/%.o)
GOBJECTS = $(LH_SOURCE_FILES:%.cpp=build_g/%.o) $(SOURCE_FILES:%.cpp=build_g/%.o)
HEADERS = $(HEADER_FILES:%.h=src/%.h) $(LH_HEADER_FILES:%.h=src/%.h)

STDLIB_ARCHS = x86 x86_avx ptx ptx_dev arm arm_android

INITIAL_MODULES = $(STDLIB_ARCHS:%=build/architecture.%.initmod.o)

NAME = HalideStar

.PHONY: all
all: bin/lib$(NAME).a bin/lib$(NAME).so include/Halide.h test_internal

bin/lib$(NAME).a: $(OBJECTS) $(INITIAL_MODULES)
	@-mkdir -p bin
	ld -r -o build/Halide.o $(OBJECTS) $(INITIAL_MODULES) $(LIBS)
	rm -f bin/lib$(NAME).a
	ar q bin/lib$(NAME).a build/Halide.o
	ranlib bin/lib$(NAME).a
    
lib/libg$(NAME).a: $(GOBJECTS) $(INITIAL_MODULES)
	@-mkdir -p lib
	ld -r -o build_g/Halide.o $(GOBJECTS) $(INITIAL_MODULES) $(LIBS)
	rm -f lib/libg$(NAME).a
	ar q lib/libg$(NAME).a build_g/Halide.o
	ranlib lib/libg$(NAME).a
    
lib/lib$(NAME).a: bin/lib$(NAME).a
	@-mkdir -p lib
	cp bin/lib$(NAME).a lib/lib$(NAME).a

bin/lib$(NAME).so: bin/lib$(NAME).a
	$(CXX) -shared $(OBJECTS) $(INITIAL_MODULES) $(LIBS) -o bin/lib$(NAME).so

include/Halide.h: $(HEADERS) bin/build_halide_h
	mkdir -p include
	cd src; ../bin/build_halide_h $(HEADER_FILES) $(LH_HEADER_FILES) > ../include/Halide.h; cd ..

bin/build_halide_h: src/build_halide_h.cpp
	g++ $< -o $@

INITMOD_OPTS_x86 = -march=corei7 -mattr=-avx
INITMOD_OPTS_ptx = -march=corei7 
INITMOD_OPTS_x86_avx = -march=corei7-avx -mattr=+avx
INITMOD_OPTS_arm = -m32 
INITMOD_OPTS_arm_android = -m32 
INITMOD_OPTS_ptx_dev = ""
INITMOD_LL_STUB_x86 = architecture.x86.stdlib.ll
INITMOD_LL_STUB_ptx = architecture.ptx.stdlib.ll
INITMOD_LL_STUB_x86_avx = architecture.x86_avx.stdlib.ll
INITMOD_LL_STUB_arm = architecture.arm.stdlib.ll
INITMOD_LL_STUB_arm_android = architecture.arm.stdlib.ll
INITMOD_LL_STUB_ptx_dev = architecture.ptx.stdlib.ll

-include $(OBJECTS:.o=.d)

build/architecture.%.initmod.cpp: bin/bitcode2cpp src/architecture.%.stdlib.cpp src/$(INITMOD_LL_STUB_%) src/architecture.posix.stdlib.cpp build/llvm_ok build/clang_ok
	@-mkdir -p build
	$(CLANG) $(INITMOD_OPTS_$*) -emit-llvm -O3 -S src/architecture.$*.stdlib.cpp -o - | \
	cat - src/$(INITMOD_LL_STUB_$*) | \
	$(LLVM_AS) -o - | \
	./bin/bitcode2cpp $* > $@

bin/bitcode2cpp: src/bitcode2cpp.cpp
	@-mkdir -p bin
	$(CXX) $< -o $@

build/architecture.%.initmod.o: build/architecture.%.initmod.cpp
	$(CXX) -c $< -o $@ 

build/%.o: src/%.cpp src/%.h build/llvm_ok
	@-mkdir -p build
	$(CXX) $(CXX_FLAGS) -c $< -o $@ -MMD -MP -MF build/$*.d -MT build/$*.o 

build_g/%.o: src/%.cpp src/%.h build/llvm_ok
	@-mkdir -p build_g
	$(CXX) $(CXX_FLAGS) -g -c $< -o $@ -MMD -MP -MF build_g/$*.d -MT build_g/$*.o 

.PHONY: clean
clean:
	rm -rf bin/*
	rm -rf build/*
	rm -rf include/*
	rm -rf lib/*
	rm -rf build_g/*

.SECONDARY:

TESTS = $(shell ls test/*.cpp)

tests: $(TESTS:test/%.cpp=test_%)

gtests: $(TESTS:test/%.cpp=gtest_%)

bin/test_internal: test/internal.cpp bin/lib$(NAME).so
	$(CXX) $(CXX_FLAGS) -rdynamic $< -Isrc -Lbin -l$(NAME) -lpthread -ldl -o $@	

bin/gtest_internal: test/internal.cpp lib/libg$(NAME).a
	$(CXX) -g -rdynamic $< -Isrc -Llib -lg$(NAME) -lpthread -ldl -o $@	

bin/test_%: test/%.cpp bin/lib$(NAME).so include/Halide.h
	$(CXX) -g $< -rdynamic -Iinclude -Lbin -l$(NAME) -lpthread -ldl -o $@	

bin/gtest_%: test/%.cpp lib/libg$(NAME).a include/Halide.h
	$(CXX) -g $< -rdynamic -Iinclude -Llib -lg$(NAME) -lpthread -ldl -o $@	

test_%: bin/test_%
	@-mkdir -p tmp
	cd tmp ; DYLD_LIBRARY_PATH=../bin LD_LIBRARY_PATH=../bin ../$<
	@-echo

gtest_%: bin/gtest_%
	@-mkdir -p tmp
	cd tmp ; DYLD_LIBRARY_PATH=../bin LD_LIBRARY_PATH=../bin ../$<
	@-echo


.PHONY: test_apps
test_apps: bin/lib$(NAME).a
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
build/clang_ok:
	@echo "Found a new enough version of clang"
	mkdir -p build
	touch build/clang_ok
else
build/clang_ok:
	@echo "Can't find clang or version of clang too old (we need 3.1 or greater):"
	@echo "You can override this check by setting CLANG_OK=y"
	echo '$(CLANG_VERSION)'
	echo $(findstring version 3,$(CLANG_VERSION))
	echo $(findstring version 3.0,$(CLANG_VERSION))
	$(CLANG) --version
	@exit 1
endif

ifdef LLVM_OK
build/llvm_ok:
	@echo "Found a new enough version of llvm"
	mkdir -p build
	touch build/llvm_ok
else
build/llvm_ok:
	@echo "Can't find llvm or version of llvm too old (we need 3.2 or greater):"
	@echo "You can override this check by setting LLVM_OK=y"
	$(LLVM_CONFIG) --version
	@exit 1
endif

