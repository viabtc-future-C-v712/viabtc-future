TARGET  := matchengine.exe
INCS = -I ../network -I ../utils
LIBS = -L ../utils -lutils -L ../network -lnetwork -Wl,-Bstatic -lev -ljansson -lmpdec -lmysqlclient -lrdkafka -lhiredis -lstdc++ -Wl,-Bdynamic -lm -lpthread -ldl -lcurl -lssl -lcrypto -lz -llz4
include ../makefile.inc
