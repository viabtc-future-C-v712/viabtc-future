TARGET  := accesshttp.exe
INCS = -I ../network -I ../utils
LIBS = -L ../utils -lutils -L ../network -lnetwork -Wl,-Bstatic -lev -ljansson -lmpdec -lrdkafka -lhiredis -Wl,-Bdynamic -lm -lpthread -ldl -lssl -lcrypto -lz
include ../makefile.inc
