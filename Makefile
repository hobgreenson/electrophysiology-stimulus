CC = g++
#UNAME_S := $(shell uname)
#ifeq (UNAME_S,Darwin) 
CFLAGS = -g -Wall -O3 -DGL_GLEXT_PROTOTYPES -DOSX
INCFLAGS = -I/usr/X11/include
LDFLAGS = -framework OpenGL -framework Cocoa\
          -framework IOKit -framework CoreVideo \
          -L"/System/Library/Frameworks/OpenGL.framework/Libraries" \
          -lGL -lGLU -lm -lglfw3 -lserial
#else
#CFLAGS = -g -Wall -O3
#INCFLAGS = -I. -I/tmp/usr/local/include 
#LDFLAGS = -L. -L/tmp/usr/local/lib -lserial -lGLEW -lGL -lGLU -lglfw3 -lX11 -lXxf86vm -lXrandr -lpthread -lXi -lXinerama -lXcursor
#endif

all: game 
game: main.o load_shader.o load_shader.h Vertex2D.h Mesh.o Mesh.h    
	$(CC) $(CFLAGS) -o game main.o load_shader.o Mesh.o $(LDFLAGS) $(INCFLAGS)
main.o: main.cpp load_shader.h Mesh.h Vertex2D.h 
	$(CC) $(CFLAGS) $(INCFLAGS) -c main.cpp
load_shader.o: load_shader.cpp load_shader.h
	$(CC) $(CFLAGS) $(INCFLAGS) -c load_shader.cpp
Mesh.o: Mesh.cpp Mesh.h Vertex2D.h 
	$(CC) $(CFLAGS) $(INCFLAGS) -c Mesh.cpp


