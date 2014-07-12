check_akdtree: check_akdtree.c libakdtree.so
	$(CC) -Wall -Wextra -std=c99 -o $@ -g -I.. $< -L. -lakdtree -lcheck

libakdtree.so:
	ln -s ../libakdtree.so.* $@

test: check_akdtree
	LD_LIBRARY_PATH="..:$$LD_LIBRARY_PATH" \
	    ./check_akdtree

all: test

clean:
	rm -f check_akdtree libakdtree.so
