all:
	gcc `pkg-config --cflags --libs glib-2.0 libgpod-1.0` -std=c99 -shared -o ipod.so ipod.c -fPIC

clean:
	-rm -rf *.so *.o
