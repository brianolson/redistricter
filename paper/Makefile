#TARGETS = fourup.pdf twoup.pdf one.pdf
SRC = head.ltx front.ltx

#all:	$(TARGETS)
all:	one.pdf

fourup.pdf:	fourup.ltx ${SRC}
	pdflatex fourup.ltx

twoup.pdf:	twoup.ltx ${SRC}
	pdflatex twoup.ltx

one.pdf:	one.ltx ${SRC}
	pdflatex one.ltx

clean:
	rm -f $(TARGETS) *.out *.aux *.log
