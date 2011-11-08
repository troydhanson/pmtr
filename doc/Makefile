all: pmtr.html

pmtr.html: pmtr.txt
	asciidoc -b html5 -a toc $<

clean: 
	rm -f pmtr.html
