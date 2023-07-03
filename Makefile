.PHONY: default engine exploit clean

default: engine exploit

exploit:
	$(MAKE) -C $@

engine:
	$(MAKE) -C $@

clean:
	$(MAKE) -C engine clean
	$(MAKE) -C exploit clean
