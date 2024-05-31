

all:

SOURCES 	:= $(wildcard src/*.cc)
HEADERS 	:= $(wildcard include/*.hh)
APPS 		:= $(wildcard apps/*.cc)
OBJECTS 	:= $(patsubst src/%.cc, build/src/%.o, ${SOURCES})
PROGRAMS	:= $(patsubst apps/%.cc, build/apps/%, ${APPS})

OMP_FLAGS 	:= -D ENABLE_OMP -fopenmp
CXXFLAGS	:= -std=c++17 -g -Wall -Wno-deprecated-declarations -I./include -I${PYBOMBS_PREFIX}/include -I${VIRTUAL_ENV}/include -I./liquid-dsp/include ${OPT_FLAGS} -Wno-class-memaccess ${OMP_FLAGS}
LDFLAGS		:= -L${VIRTUAL_ENV}/lib -L./build/lib -L./liquid-dsp
LIBS		:= -lliquid -lfftw3f -pthread -lzmq -lczmq -luhd -lboost_system -lyaml

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
		mkdir -p ./build/src; \
		mkdir -p ./build/apps; \
		mkdir -p ./build/include; \
		mkdir -p ./build/lib; \
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

build/%.o : %.cc | check-virtualenv check-pybombs
	g++ ${CXXFLAGS} -MD -MP $< -c -o $@

${PROGRAMS} : build/% : %.cc | ${OBJECTS}
	g++ -I${PYBOMBS_PREFIX}/include ${CXXFLAGS} -L${PYBOMBS_PREFIX}/lib ${LDFLAGS} $< ${OBJECTS} -o $@ ${LIBS}

all						: builddir ${PROGRAMS}

echo_debug:
	@echo "SOURCES = ${SOURCES}"
	@echo "HEADERS = ${HEADERS}"
	@echo "APPS = ${APPS}"
	@echo "OBJECTS = ${OBJECTS}"
	@echo "PROGRAMS = ${PROGRAMS}"
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
