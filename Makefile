.PHONY: bootstrap test build flash monitor

bootstrap:
	./scripts/bootstrap.sh

test:
	./scripts/test.sh

build:
	./scripts/build.sh

flash:
	./scripts/flash.sh

monitor:
	./scripts/monitor.sh

