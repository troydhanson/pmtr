all: pmtr.html

pmtr.html: pmtr.txt
	asciidoc -a toc2 $<

TMP=/tmp/pmtr-gh-pages
stage:
	mkdir -p ${TMP}
	rm -if ${TMP}/*
	cp pmtr.html ${TMP}/index.html
	cp *.png ${TMP}

clean: 
	rm -f pmtr.html
