

all:

SOURCES 	:= $(wildcard src/*.cc)
HEADERS 	:= $(wildcard include/*.hh)
APPS 		:= $(wildcard apps/*.cc)
TESTER		:= $(wildcard test/*.cc)
OBJECTS 	:= $(patsubst src/%.cc, build/_cpp/src/%.o, ${SOURCES})
PROGRAMS	:= $(patsubst apps/%.cc, build/_cpp/apps/%, ${APPS})
TESTS		:= $(patsubst test/%.cc, build/_cpp/test/%, ${TESTER})

_C_OBJS		:= $(patsubst src/%.cc, build/_c/src/%.o, ${SOURCES})
_C_OBJS		+= $(patsubst apps/%.cc, build/_c/apps/%.o, ${APPS})
_C_OBJS		+= $(patsubst test/%.cc, build/_c/test/%.o, ${TESTER})
_C_TEST 	:= $(patsubst test/%.cc, build/_c/test/%, ${TESTER})
_C_PROGS	:= $(patsubst apps/%.cc, build/_c/apps/%, ${APPS})


OMP_FLAGS 	:= -D ENABLE_OMP -fopenmp
CXXFLAGS	:= -std=c++17 -g -Wall -Wno-deprecated-declarations -I./include -I${PYBOMBS_PREFIX}/include -I${VIRTUAL_ENV}/include -I./liquid-dsp/include ${OPT_FLAGS} -Wno-class-memaccess ${OMP_FLAGS}
CFLAGS		:= -std=gnu11 -g -Wall -Wno-deprecated-declarations -I./include -I${PYBOMBS_PREFIX}/include -I${VIRTUAL_ENV}/include -I./liquid-dsp/include ${OPT_FLAGS} ${OMP_FLAGS}
LDFLAGS		:= -L${VIRTUAL_ENV}/lib -L./build/lib -L./liquid-dsp
LIBS		:= -lm -lliquid -lfftw3f -pthread -lzmq -lczmq -luhd -lboost_system -lyaml

.phony: clean echo_debug

liquid-dsp/configure    : 
	cd ./liquid-dsp && ./bootstrap.sh

liquid-dsp/makefile     : liquid-dsp/configure
	cd ./liquid-dsp && ./configure --prefix ${VIRTUAL_ENV}

liquid-dsp/libliquid.so : liquid-dsp/makefile
	cd ./liquid-dsp && $(MAKE) all

liquid					: liquid-dsp/libliquid.so builddir
	cp ./liquid-dsp/libliquid.so ./build/lib/
	cd ./liquid-dsp && $(MAKE) install

clean-liquid			:
	cd ./liquid-dsp && $(MAKE) distclean

builddir				:
	if [ ! -d "./build/src" ]; then \
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

clean					:
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

build/_cpp/%.o : %.cc | check-virtualenv check-pybombs
	g++ ${CXXFLAGS} -MD -MP $< -c -o $@

build/_c/%.o : %.cc | check-virtualenv check-pybombs
	-gcc -x c ${CFLAGS} -MD -MP $< -c -o $@

${PROGRAMS} : build/_cpp/% : %.cc | ${OBJECTS}
	g++ -I${PYBOMBS_PREFIX}/include ${CXXFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} $< ${OBJECTS} -o $@ ${LIBS}

${TESTS} : build/_cpp/% : %.cc | ${OBJECTS}
	g++ -I${PYBOMBS_PREFIX}/include ${CXXFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} $< ${OBJECTS} -o $@ ${LIBS}

${_C_PROGS} : build/_c/% : %.cc | ${_C_OBJS}
	-gcc -x c -I${PYBOMBS_PREFIX}/include ${CFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} ${_C_OBJS} -o $@ ${LIBS}

${_C_TEST} : build/_c/% : %.cc | ${_C_OBJS}
	-gcc -x c -I${PYBOMBS_PREFIX}/include ${CFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} ${_C_OBJS} -o $@ ${LIBS}

all						: builddir ${PROGRAMS} ${_C_PROGS}

test 					: builddir ${TESTS} ${_C_TEST}

echo_debug:
	@echo "SOURCES = ${SOURCES}"
	@echo "HEADERS = ${HEADERS}"
	@echo "APPS = ${APPS}"
	@echo "OBJECTS = ${OBJECTS}"
	@echo "_C_OBJS = ${_C_OBJS}"
	@echo "PROGRAMS = ${PROGRAMS}"
	@echo "_C_PROGS = ${_C_PROGS}"
	@echo "TESTS = ${TESTS}"
	@echo "_C_TEST = ${_C_TEST}"
	@echo "OMP_FLAGS = ${OMP_FLAGS}"
	@echo "CXXFLAGS = ${CXXFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "LIBS = ${LIBS}"


install					: check-virtualenv all
	cp ./build/apps/* ${VIRTUAL_ENV}/bin/

uninstall				: check-virtualenv
	rm $(patsubst apps/%.cc, ${VIRTUAL_ENV}/bin/%, ${APPS})
	cd ./liquid-dsp && make uninstall

-include $(OBJECTS:.o=.d)
-include $(_C_OBJS:.o=.d)
