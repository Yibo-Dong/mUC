ifeq ($(SOLVER),MINISAT)
SOLVER_FLAG=-DMINISAT
else ifeq ($(SOLVER),GLUCOSE4)
SOLVER_FLAG=-DGLUCOSE
else
SOLVER_FLAG=-DGLUCOSE
endif

ifeq ($(TRAIL),ON)
TRAIL_FLAG=-DTRAIL
endif

ifeq ($(DIVE),ON)
DIVE_FLAG=-DDIVE
endif


ifeq ($(DEPTH),ON)
DEPTH_FLAG=-DDEPTH
else
DEPTH_FLAG=
endif

ifeq ($(SCORE),ON)
SCORE_FLAG=-DSCORE
else
SCORE_FLAG=
endif

ifeq ($(SCORE_REV),ON)
SCORE_REV_FLAG=-DSCORE_REVERSE
else
SCORE_REV_FLAG=
endif

ifeq ($(SCORE_ABS),ON)
SCORE_OPTION_FLAG=-DSCORE_ABS
else
SCORE_OPTION_FLAG=
endif

ifeq ($(SCORE_DECAY),ON)
SCORE_DECAY_FLAG=-DSCORE_DECAY
else
SCORE_DECAY_FLAG=
endif





ifeq ($(FRESHUC),ON)
FRESHUC_FLAG=-DFRESH_UC
else
FRESHUC_FLAG=
endif

ifeq ($(FRESHU),ON)
FRESH_U_FLAG=-DFRESH_U
else
FRESH_U_FLAG=
endif


ifeq ($(CONTAINER),PQUEUE)
CONTAINER_FLAG=-DPQUEUE
else
CONTAINER_FLAG=-DSTACK
endif


ifeq ($(PARTIAL),ON)
PARTIAL_FLAG=-DPARTIAL
else
PARTIAL_FLAG=
endif

ifeq ($(INC_SAT),ON)
INC_SAT_FLAG=-DINC_SAT
else
INC_SAT_FLAG=
endif



ifeq ($(SIMPU_INC),ON)
SIMPU_INC_FLAG=-DSIMPU_INC
else
SIMPU_INC_FLAG=
endif



SEED ?= 0

ifeq ($(SEED),0)
RANDOM_FLAG= 
else
RANDOM_FLAG=-DRANDSEED=$(SEED)
endif



ifeq ($(ASSU),IRRI)
ASSUM_FLAG=-DASS_IRRI
else ifeq ($(ASSU),IIRR)
ASSUM_FLAG=-DASS_IIRR
else ifeq ($(ASSU),IRIR)
ASSUM_FLAG=-DASS_IRIR
else ifeq ($(ASSU),RIRI)
ASSUM_FLAG=-DASS_RIRI
else ifeq ($(ASSU),RRII)
ASSUM_FLAG=-DASS_RRII
else ifeq ($(ASSU),RIIR)
ASSUM_FLAG=-DASS_RIIR
else ifeq ($(ASSU),SHUFFLE_INTER)
ASSUM_FLAG=-DASS_SHUFFLE_INTER
else
ASSUM_FLAG=
endif


ifeq ($(INTER_ORDER),LAST_FIRST)
INTER_ORDER_FLAG=-DLAST_FIRST
else
INTER_ORDER_FLAG=
endif

ifeq ($(FRONT),ON)
FRONT_FLAG=-DFRONT_FLAG
else
FRONT_FLAG=
endif

ifeq ($(PICK_GUIDE), RANDOM)
GUIDE_FLAG=-DPICK_GUIDE_RANDOM
else
GUIDE_FLAG=
endif

MAXNI ?= -1
ifeq ($(MAXNI), -1)
MAXNI_FLAG=
else
MAXNI_FLAG=-DMAXNI=$(MAXNI)
endif

COOK ?= -1
ifeq ($(COOK), -1)
COOK_FLAG=
else
COOK_FLAG=-DCOOK=$(COOK)
endif

ifeq ($(COOK_LIGHT), ON)
COOK_LIGHT_FLAG=-DCOOK_LIGHT
else
COOK_LIGHT_FLAG=
endif

ifeq ($(STAT), OFF)
STAT_FLAG=
else
STAT_FLAG=-DSTAT
endif



ifeq ($(SOLVER),MINISAT)
SOURCES = src/model/aiger.c src/model/model.cpp\
			src/checker/carChecker.cpp src/checker/bmcChecker.cpp \
			src/solver/carsolver.cpp src/solver/mainsolver.cpp src/solver/newpartialsolver.cpp \
			src/utils/data_structure.cpp src/utils/utility.cpp src/solver/implysolver.cpp\
			src/newmain.cpp \
			src/sat/minisat/core/Solver.cc src/sat/minisat/utils/Options.cc src/sat/minisat/utils/System.cc		
INCLUDE_DIRS = -I./src -I./src/model -I./src/checker -I./src/debug -I./src/sat -I./src/solver -I./src/utils -I./src/sat/minisat -I./src/sat/minisat/core -I./src/sat/minisat/utils
OBJECTS = carsolver.o implysolver.o newpartialsolver.o mainsolver.o model.o newmain.o utility.o data_structure.o aiger.o\
	Solver.o Options.o System.o carChecker.o bmcChecker.o
CFLAG = $(INCLUDE_DIRS) -D__STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -c -g
else ifeq ($(SOLVER),GLUCOSE4)
SOURCES = src/model/aiger.c src/model/model.cpp\
			src/checker/carChecker.cpp src/checker/bmcChecker.cpp \
			src/solver/carsolver.cpp src/solver/mainsolver.cpp src/solver/newpartialsolver.cpp \
			src/utils/data_structure.cpp src/utils/utility.cpp src/solver/implysolver.cpp\
			src/newmain.cpp \
			src/sat/glucose-4.2.1/core/Solver.cc src/sat/glucose-4.2.1/utils/Options.cc src/sat/glucose-4.2.1/utils/System.cc src/sat/glucose-4.2.1/core/lcm.cc
INCLUDE_DIRS = -I./src -I./src/model -I./src/checker -I./src/debug -I./src/sat -I./src/solver -I./src/utils -I./src/sat/glucose-4.2.1 -I./src/sat/glucose-4.2.1/core -I./src/sat/glucose-4.2.1/utils
OBJECTS = carsolver.o implysolver.o newpartialsolver.o mainsolver.o model.o newmain.o utility.o data_structure.o aiger.o\
	Solver.o Options.o System.o carChecker.o bmcChecker.o lcm.o
CFLAG = $(INCLUDE_DIRS) -D__STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -c -g
else
SOURCES = src/model/aiger.c src/model/model.cpp\
			src/checker/carChecker.cpp src/checker/bmcChecker.cpp \
			src/solver/carsolver.cpp src/solver/mainsolver.cpp src/solver/newpartialsolver.cpp \
			src/utils/data_structure.cpp src/utils/utility.cpp src/solver/implysolver.cpp\
			src/newmain.cpp \
			src/sat/glucose/core/Solver.cc src/sat/glucose/utils/Options.cc src/sat/glucose/utils/System.cc
INCLUDE_DIRS = -I./src -I./src/model -I./src/checker -I./src/debug -I./src/sat -I./src/solver -I./src/utils -I./src/sat/glucose -I./src/sat/glucose/core -I./src/sat/glucose/utils
OBJECTS = carsolver.o implysolver.o newpartialsolver.o mainsolver.o model.o newmain.o utility.o data_structure.o aiger.o\
	Solver.o Options.o System.o carChecker.o bmcChecker.o
CFLAG = $(INCLUDE_DIRS) -D__STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -c -g
endif



# consider using -fprofile-generate and -fprofile-use
OPTFLAG = -Ofast -march=native -frename-registers -funroll-loops -fno-signed-zeros

LFLAG = -g -lz -lpthread 

GCC = gcc

GXX = g++

TARGET ?= caramel

OPTIONS=$(TRAIL_FLAG) ${RANDOM_FLAG} ${CONTAINER_FLAG} ${FRESHUC_FLAG} ${FRESH_U_FLAG} ${PARTIAL_FLAG} ${SIMPU_INC_FLAG} ${ASSUM_FLAG} ${DIVE_FLAG} ${GUIDE_FLAG} ${SOLVER_FLAG} ${INTER_ORDER_FLAG} ${MAXNI_FLAG} ${COOK_FLAG} ${COOK_LIGHT_FLAG} $(SCORE_FLAG) $(SCORE_OPTION_FLAG) $(SCORE_REV_FLAG) $(FRONT_FLAG) $(SCORE_DECAY_FLAG) ${DEPTH_FLAG} ${STAT_FLAG}

caramel:$(SOURCES)
	$(GCC) ${OPTFLAG} ${OPTIONS} $(CFLAG) $(SOURCES)
	$(GXX) ${OPTFLAG} -g -o ${TARGET} $(OBJECTS) $(LFLAG)

clean: 
	rm *.o
	rm caramel

.PHONY: caramel
