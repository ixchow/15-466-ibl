
CPP = g++ -Wall -Werror -std=c++14

../dist/cape_hill_512.png : cape_hill_4k.hdr hdr_to_cube
	./hdr_to_cube '$<' 512 '$@'

cape_hill_4k.hdr :
	wget 'https://hdrihaven.com/hdri/download.php?h=cape_hill&r=4k' -O'$@'

hdr_to_cube : objs/hdr_to_cube.o objs/load_hdr.o objs/load_save_png.o
	$(CPP) -o '$@' $^ -lpng -lz

blur_cube : objs/blur_cube.o objs/load_save_png.o
	$(CPP) -o '$@' $^ -lpng -lz

objs/blur_cube.o : blur_cube.cpp ../load_save_png.hpp ../rgbe.hpp
	mkdir -p objs
	$(CPP) -c -o '$@' '$<' -I..

objs/hdr_to_cube.o : hdr_to_cube.cpp load_hdr.hpp ../load_save_png.hpp ../rgbe.hpp
	mkdir -p objs
	$(CPP) -c -o '$@' '$<' -I..

objs/load_hdr.o : load_hdr.cpp load_hdr.hpp
	mkdir -p objs
	$(CPP) -c -o '$@' '$<'

objs/load_save_png.o : ../load_save_png.cpp ../load_save_png.hpp
	mkdir -p objs
	$(CPP) -c -o '$@' '$<'
