TARGET = danish
OBJS = main.o page.o ssh.o site.opp

CFLAGS = -g -Wall -pipe $(shell pkg-config --cflags gtk+-3.0 vte-2.91 gthread-2.0 tinyxml)
CXXFLAGS = -g -Wall -pipe $(shell pkg-config --cflags gtk+-3.0 vte-2.91 gthread-2.0 tinyxml)
LDFLAGS +=  -g -lstdc++ -lssh2 $(shell pkg-config --libs gtk+-3.0 vte-2.91 gthread-2.0 tinyxml)
danish:main.o page.o ssh.o site.opp
	gcc $^ $(LDFLAGS) -o $@

page.o: page.c page.h site.h ssh.h makefile
	gcc -c $(CFLAGS) -o $@ $<

ssh.o:ssh.c ssh.h page.h makefile
	gcc -c $(CFLAGS) -lssh2 -o $@ $<
	
site.opp: site.cpp site.h ssh.h makefile
	gcc -c $(CXXFLAGS) -o $@ $<

install: ${TARGET}
	sudo desktop-file-install danish.desktop
	sudo cp ${TARGET} /usr/local/bin/
	mkdir -p ${HOME}/.danish/
	cp -r res ${HOME}/.danish/
	if [ ! -f ${HOME}/.danish/site.xml ]; then cp site.xml.tmpl ${HOME}/.danish/site.xml; fi
clean:
	rm -rf $(TARGET) $(OBJS)
	rm -rf cscope*

