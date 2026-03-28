# Compilatio instrumentorum ddiff et dpatch

CC      = cc
CFLAGS  = -O2 -Wall -Wextra -Wpedantic
OMNIA   = ddiff dpatch

omnia: $(OMNIA)

ddiff: ddiff.c
	$(CC) $(CFLAGS) -o $@ $<

dpatch: dpatch.c
	$(CC) $(CFLAGS) -o $@ $<

proba: $(OMNIA)
	@rm -rf probata/applicatio
	@mkdir probata/applicatio
	@cp probata/veterum/* probata/applicatio/
	(cd probata && diff -ruN veterum novum) | ./ddiff 2>/dev/null | \
		(cd probata/applicatio && ../../dpatch) 2>/dev/null
	@diff -r probata/novum probata/applicatio > /dev/null \
		&& echo "PROBATIO SVCCESSIT" \
		|| (echo "PROBATIO FALLIT"; diff -r probata/novum probata/applicatio; exit 1)
	@rm -rf probata/applicatio

purga:
	rm -f $(OMNIA)

.PHONY: omnia purga proba
