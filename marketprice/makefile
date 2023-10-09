TARGET  := marketprice.exe
INCS = -I ../network -I ../utils
LIBS = -L ../utils -lutils -L ../network -lnetwork -Wl,-Bstatic -lev -ljansson -lmpdec -lrdkafka -llz4 -lhiredis -lstdc++ -Wl,-Bdynamic -lm -lpthread -ldl -lcurl -lssl -lcrypto -lz
include ../makefile.inc
