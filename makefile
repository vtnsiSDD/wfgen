

all:

SOURCES 	:= $(wildcard src/*.cc)
HEADERS 	:= $(wildcard include/*.hh)
APPS 		:= $(wildcard apps/*.cpp)
_C_APPS 	:= $(wildcard apps/*.cc)
TESTER		:= $(wildcard test/*.cc)
OBJECTS 	:= $(patsubst src/%.cc, build/_cpp/src/%.o, ${SOURCES})
PROGRAMS	:= $(patsubst apps/%.cpp, build/_cpp/apps/%, ${APPS})
TESTS		:= $(patsubst test/%.cc, build/_cpp/test/%, ${TESTER})

_C_OBJS		:= $(patsubst src/%.cc, build/_c/src/%.o, ${SOURCES})
_C_TOBJS	:= $(patsubst test/%.cc, build/_c/test/%.o, ${TESTER})
_C_TEST 	:= $(patsubst test/%.cc, build/_c/test/%, ${TESTER})
_C_POBJS	:= $(patsubst apps/%.cc, build/_c/apps/%.o, ${_C_APPS})
_C_PROGS	:= $(patsubst apps/%.cc, build/_c/apps/%, ${_C_APPS})


OMP_FLAGS 	:= -D ENABLE_OMP -fopenmp
CXXFLAGS	:= -std=c++17 -g -Wall -fPIC -Wno-deprecated-declarations -I./include -I${PYBOMBS_PREFIX}/include -I${VIRTUAL_ENV}/include -I./liquid-dsp/include ${OPT_FLAGS} -Wno-class-memaccess ${OMP_FLAGS}
CFLAGS		:= -std=gnu11 -g -Wall -fPIC -Wno-deprecated-declarations -I./include -I${PYBOMBS_PREFIX}/include -I${VIRTUAL_ENV}/include -I./liquid-dsp/include ${OPT_FLAGS} ${OMP_FLAGS}
LDFLAGS		:= -L${VIRTUAL_ENV}/lib -L./liquid-dsp
LIBS		:= -lm -lliquid -lfftw3f -pthread -lzmq -lczmq -luhd -lboost_system -lyaml

.phony: clean echo_debug

liquid-dsp/configure    : 
	cd ./liquid-dsp && ./bootstrap.sh

liquid-dsp/makefile     : liquid-dsp/configure
	cd ./liquid-dsp && ./configure --prefix ${VIRTUAL_ENV}

liquid-dsp/libliquid.so : liquid-dsp/makefile
	cd ./liquid-dsp && $(MAKE) all

liquid					: liquid-dsp/libliquid.so builddir
	cp ./liquid-dsp/libliquid.so ./build/_c/lib/
	cp ./liquid-dsp/libliquid.so ./build/_cpp/lib/

clean-liquid			:
	cd ./liquid-dsp && $(MAKE) distclean

builddir				:
	if [ ! -d "./build/_cpp/src" ]; then \
		mkdir -p ./build/_cpp/src; \
		mkdir -p ./build/_c/src; \
		mkdir -p ./build/_cpp/apps; \
		mkdir -p ./build/_c/apps; \
		mkdir -p ./build/_cpp/include; \
		mkdir -p ./build/_c/include; \
		mkdir -p ./build/_cpp/lib; \
		mkdir -p ./build/_c/lib; \
		mkdir -p ./build/_cpp/test; \
		mkdir -p ./build/_c/test; \
	fi

clean					: clean-liquid
	rm -rf ./build

check-virtualenv		:
ifndef VIRTUAL_ENV
	$(error VIRTUAL_ENV is not set)
endif
	if [ ! -d "${VIRTUAL_ENV}/include" ]; then mkdir -p ${VIRTUAL_ENV}/include; fi

check-pybombs:
ifndef PYBOMBS_PREFIX
	$(error PYBOMBS_PREFIX is not set)
endif

build/_cpp/%.o : %.cc | check-virtualenv check-pybombs builddir
	g++ ${CXXFLAGS} -MD -MP $< -c -o $@

build/_c/%.o : %.cc | check-virtualenv check-pybombs builddir
	-gcc -x c ${CFLAGS} -MD -MP $< -c -o $@

build/_cpp/lib/libwfgen_cpp.a : liquid ${OBJECTS}
	ar -rc build/_cpp/lib/libwfgen_cpp.a ${OBJECTS}
build/_c/lib/libwfgen_c.a : liquid ${_C_OBJS}
	ar -rc build/_c/lib/libwfgen_c.a ${_C_OBJS}
build/_cpp/lib/libwfgen_cpp.so : liquid ${OBJECTS}
	g++ -shared ${LDFLAGS} -o build/_cpp/lib/libwfgen_cpp.so ${LIBS} ${OBJECTS}
build/_c/lib/libwfgen_c.so : liquid ${_C_OBJS}
	gcc -shared ${LDFLAGS} -o build/_c/lib/libwfgen_c.so ${LIBS} ${_C_OBJS}

wf_gen_cpp_libs : build/_cpp/lib/libwfgen_cpp.a build/_cpp/lib/libwfgen_cpp.so
wf_gen_c_libs : build/_c/lib/libwfgen_c.a build/_c/lib/libwfgen_c.so

${PROGRAMS} : build/_cpp/% : %.cpp | ${OBJECTS} wf_gen_cpp_libs
	g++ -I${PYBOMBS_PREFIX}/include ${CXXFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} -L./build/_cpp/lib $< -o $@ -lwfgen_cpp ${LIBS}

${TESTS} : build/_cpp/% : %.cc | ${OBJECTS} wf_gen_cpp_libs
	-g++ -I${PYBOMBS_PREFIX}/include ${CXXFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} -L./build/_cpp/lib $< -o $@ -lwfgen_cpp ${LIBS}

${_C_PROGS} : build/_c/% : %.cc | ${_C_OBJS} ${_C_POBJS} wf_gen_c_libs
	-gcc -x c -I${PYBOMBS_PREFIX}/include ${CFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} -L./build/_c/lib ${_C_POBJS} -o $@ -lwfgen_c ${LIBS}

${_C_TEST} : build/_c/% : %.cc | ${_C_OBJS} ${_C_TOBJS} wf_gen_c_libs
	gcc -I${PYBOMBS_PREFIX}/include ${CFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} -L./build/_c/lib ${_C_TOBJS} -o $@ -lwfgen_c ${LIBS}

run_tests: ${TESTS} ${_C_TEST}
	for f in ${TESTS}; do LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:build/_cpp/lib  ./$$f; done
	for f in ${_C_TEST}; do LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:build/_c/lib  ./$$f; done

all						: builddir liquid wf_gen_cpp_libs wf_gen_c_libs ${PROGRAMS} ${_C_PROGS}

test 					: builddir ${TESTS} ${_C_TEST}  run_tests

# python                  : 

echo_debug:
	@echo "SOURCES = ${SOURCES}"
	@echo "HEADERS = ${HEADERS}"
	@echo "APPS = ${APPS}"
	@echo "OBJECTS = ${OBJECTS}"
	@echo "_C_OBJS = ${_C_OBJS}"
	@echo "_C_POBJS = ${_C_POBJS}"
	@echo "_C_TOBJS = ${_C_TOBJS}"
	@echo "PROGRAMS = ${PROGRAMS}"
	@echo "_C_PROGS = ${_C_PROGS}"
	@echo "TESTS = ${TESTS}"
	@echo "_C_TEST = ${_C_TEST}"
	@echo "OMP_FLAGS = ${OMP_FLAGS}"
	@echo "CXXFLAGS = ${CXXFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "LIBS = ${LIBS}"


install					: check-virtualenv all
	cd ./liquid-dsp && $(MAKE) install
	cp ./build/_cpp/apps/* ${VIRTUAL_ENV}/bin/
	cp ./build/_cpp/lib/libwfgen* ${VIRTUAL_ENV}/lib/
	cp ./build/_c/lib/libwfgen* ${VIRTUAL_ENV}/lib/

uninstall				: check-virtualenv
	rm -f $(patsubst apps/%.cc, ${VIRTUAL_ENV}/bin/%, ${APPS})
	cd ./liquid-dsp && $(MAKE) uninstall

-include $(OBJECTS:.o=.d)
-include $(_C_OBJS:.o=.d)
-include $(_C_POBJS:.o=.d)
-include $(_C_TOBJS:.o=.d)
