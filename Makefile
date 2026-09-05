.PHONY: bootstrap test build flash monitor provision

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

provision:
	./scripts/provision.sh
