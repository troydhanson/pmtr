all: pmtr.html

pmtr.html: pmtr.txt
	asciidoc -b html5 $<
	#asciidoc -b html5 -a toc -a theme=flask $<

clean: 
	rm -f pmtr.html
