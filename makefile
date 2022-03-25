TARGET = danioc
OBJS = main.o page.o ssh.o site.opp

HEADER = config.h
CFLAGS = -g -Wall -pipe $(shell pkg-config --cflags gtk+-3.0 vte-2.91 gthread-2.0 tinyxml)
CXXFLAGS = -g -Wall -pipe $(shell pkg-config --cflags gtk+-3.0 vte-2.91 gthread-2.0 tinyxml)
LDFLAGS += -lstdc++ $(shell pkg-config --libs gtk+-3.0 vte-2.91 gthread-2.0 tinyxml)

$(TARGET): $(OBJS)
	gcc $^ $(LDFLAGS) -o $@

%.o: %.c %.h ${HEADER} makefile
	gcc -c $(CFLAGS) -o $@ $<

%.opp: %.cpp %.h ${HEADER} makefile
	g++ -c $(CXXFLAGS) -o $@ $<

install: ${TARGET}
	sudo desktop-file-install danioc.desktop
	sudo cp ${TARGET} /usr/local/bin/
	mkdir -p ${HOME}/.danioc/
	cp -r res ${HOME}/.danioc/
	if [ ! -f ${HOME}/.danioc/site.xml ]; then cp site.xml.tmpl ${HOME}/.danioc/site.xml; fi
	rm -rf $(TARGET) $(OBJS)
	rm -rf cscope*
clean:
	rm -rf $(TARGET) $(OBJS)
	rm -rf cscope*

