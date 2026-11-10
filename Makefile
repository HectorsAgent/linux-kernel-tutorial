.PHONY: all demo test clean

all demo test clean:
	$(MAKE) -C network $@
