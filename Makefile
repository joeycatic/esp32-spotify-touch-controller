.PHONY: bootstrap test build flash monitor provision build-idf flash-idf

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

# Source-built ESP-IDF target for the 7B; see firmware/idf/README.md.
build-idf:
	./scripts/build-idf.sh

flash-idf:
	./scripts/flash-idf.sh
