all: pmtr.html

pmtr.html: pmtr.txt
	asciidoc -a toc2 $<

TMP=/tmp/pmtr-gh-pages
stage:
	mkdir -p ${TMP}
	rm -if ${TMP}/*
	cp LICENSE.txt ${TMP}
	cp *.html ${TMP}

clean: 
	rm -f pmtr.html
