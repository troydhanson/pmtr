all: proctab.html

proctab.html: proctab.txt
	asciidoc -a toc2 $<

TMP=/tmp/proctab-gh-pages
stage:
	mkdir -p ${TMP}
	rm -if ${TMP}/*
	cp LICENSE.txt ${TMP}
	cp *.html ${TMP}

clean: 
	rm -f proctab.html
