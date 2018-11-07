
CPP = g++ -Wall -Werror -std=c++14

hdr_to_cube : hdr_to_cube.o load_hdr.o load_save_png.o
	$(CPP) -o '$@' $^ -lpng -lz

hdr_to_cube.o : hdr_to_cube.cpp load_hdr.hpp load_save_png.hpp rgbe.hpp
	$(CPP) -c -o '$@' '$<'

load_hdr.o : load_hdr.cpp load_hdr.hpp
	$(CPP) -c -o '$@' '$<'

load_save_png.o : load_save_png.cpp load_save_png.hpp
	$(CPP) -c -o '$@' '$<'
