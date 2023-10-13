.POSIX:
CXX=g++ -std=gnu++17
CXXFLAGS=-Wall -Wextra -Weffc++ -Wshadow -Wconversion -O3 -flto
OBJ=arg.o bmp.o cmd.o image.o lump.o sclumpy.o script.o tokenizer.o spray.o \
 stringutils.o wad.o

sclumpy: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) -lm -lstdc++fs

arg.o: arg.cpp arg.h
bmp.o: bmp.cpp bmp.h
cmd.o: cmd.cpp cmd.h
image.o: image.cpp bmp.h byte.h cmd.h image.h
lump.o: lump.cpp cmd.h wad.h
sclumpy.o: sclumpy.cpp arg.h cmd.h script.h spray.h
script.o: script.cpp image.h script.h tokenizer.h stringutils.h wad.h
spray.o: spray.cpp image.h wad.h
stringutils.o: stringutils.cpp stringutils.h
tokenizer.o: tokenizer.cpp script.h tokenizer.h
wad.o: wad.cpp byte.h cmd.h wad.h

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) sclumpy
