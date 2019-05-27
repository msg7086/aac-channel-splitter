default: aac_split.exe

aac_split.exe: aac_split.o
	g++ -static -static-libgcc -static-libstdc++ -o $@ $<
	strip $@

aac_split.o: aac_split.cpp
	g++ -O2 --std=c++11 -c aac_split.cpp