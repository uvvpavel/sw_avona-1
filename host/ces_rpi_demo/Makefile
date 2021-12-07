all: ww_demo ww_demo_root

ww_demo: ww_demo.c
	gcc -Wall -pthread -o ww_demo ww_demo.c -lpigpiod_if2 -lrt


ww_demo_root: ww_demo_root.c
	gcc -Wall -pthread -o ww_demo_root ww_demo_root.c -lpigpio -lrt -lm


