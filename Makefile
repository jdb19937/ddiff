default:
	make -f Faceplica

%:
	make -f Faceplica $(MAKECMDGOALS)

.PHONY: % default
