NAME?=	lection

TMP=	$(NAME).aux $(NAME).log $(NAME).nav $(NAME).out $(NAME).snm \
	$(NAME).toc $(NAME).vrb

.MAIN: $(NAME).pdf

.SUFFIXES:	.pdf .tex
.tex.pdf:
	pdflatex -file-line-error -halt-on-error ${.IMPSRC}

clean:
	rm -f -- $(TMP) texput.log

cleanall: clean
	rm -f $(NAME).pdf
