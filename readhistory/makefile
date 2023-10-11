TARGET  := readhistory.exe
INCS = -I ../network -I ../utils
LIBS = -L ../utils -lutils -L ../network -lnetwork -Wl,-Bstatic -lev -ljansson -lmpdec -lmysqlclient -lhiredis -lstdc++ -Wl,-Bdynamic -lm -lpthread -ldl -lz -lssl -lcrypto
include ../makefile.inc
