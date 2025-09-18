# DIGINET DNS API Client using libcurl

CC ?= cc
CFLAGS ?= -O2 -Wall
TARGET = gidinet
SOURCE = main.c

# Detect curl library location (supports both Homebrew and system curl)
CURL_CFLAGS := $(shell curl-config --cflags 2>/dev/null || echo "")
CURL_LIBS := $(shell curl-config --libs 2>/dev/null || echo "-lcurl")

# Build the client
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(CURL_CFLAGS) -o $(TARGET) $(SOURCE) $(CURL_LIBS)
	strip $(TARGET)

.PHONY: clean install test help strip

clean:
	rm -f $(TARGET) *.o

# Strip symbols for smaller binary size
strip: $(TARGET)
	strip $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Test with sample data (requires valid credentials)
test: $(TARGET)
	@echo "Testing DIGINET DNS API client commands..."
	@echo ""
	@echo "Update command example:"
	@echo "./$(TARGET) update --username YOUR_USER --passwordB64 YOUR_PASS_B64 \\"
	@echo "  --oldDomain example.com --oldHost test --oldType A --oldData 1.2.3.4 --oldTTL 300 --oldPriority 0 \\"
	@echo "  --newDomain example.com --newHost test --newType A --newData 5.6.7.8 --newTTL 300 --newPriority 0"
	@echo ""
	@echo "Add command example:"
	@echo "./$(TARGET) add --username YOUR_USER --passwordB64 YOUR_PASS_B64 \\"
	@echo "  --domain example.com --host new --type A --data 9.8.7.6 --ttl 300 --priority 0"
	@echo ""
	@echo "Delete command example:"
	@echo "./$(TARGET) delete --username YOUR_USER --passwordB64 YOUR_PASS_B64 \\"
	@echo "  --domain example.com --host test --type A --data 1.2.3.4 --ttl 300 --priority 0"
	@echo ""
	@echo "List command example:"
	@echo "./$(TARGET) list --username YOUR_USER --passwordB64 YOUR_PASS_B64 --domain example.com"

help:
	@echo "DIGINET DNS API Client (libcurl-based) - QuickServiceBox DNS Management"
	@echo ""
	@echo "Targets:"
	@echo "  $(TARGET)    - Build the DIGINET DNS API client (with symbols stripped)"
	@echo "  clean        - Remove built files"
	@echo "  strip        - Strip symbols from existing binary"
	@echo "  install      - Install to /usr/local/bin"
	@echo "  test         - Show test command examples"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Commands:"
	@echo "  update       - Update an existing DNS record"
	@echo "  add          - Add a new DNS record"
	@echo "  delete       - Delete an existing DNS record"
	@echo "  list         - List DNS records for a domain"
	@echo ""
	@echo "Usage:"
	@echo "  ./$(TARGET) <command> [options]"
	@echo "  ./$(TARGET) <command> --help    (for command-specific help)"
