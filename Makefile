.PHONY: all
all: simrv32im asrv32im

.PHONY: simrv32im
simrv32im:
	$(MAKE) -C simrv32im

.PHONY: asrv32im
asrv32im:
	$(MAKE) -C asrv32im

.PHONY: clean
clean:
	$(MAKE) -C simrv32im clean
	$(MAKE) -C asrv32im clean
	rm -rf bin
