# GIDINET DNS API Client v0.1

This project is a C client using libcurl to interact with the QuickServiceBox DNS API. It supports multiple DNS record operations through a command-based interface with clean JSON output for automation.

## Features

- **Multiple Commands**: `update`, `add`, `delete`, `list`, `version`
- **JSON Output**: Clean JSON responses perfect for automation and scripting
- **jq Compatible**: Error-free parsing with tools like jq
- **Human-readable Results**: Translates API result codes to English messages
- **Parameter Validation**: Clear error messages for missing or invalid parameters
- **SSL/TLS Security**: Production-ready HTTPS communication
- **Optimized Build**: Symbol-stripped binary for minimal size
- **Cross-platform**: Works with both Homebrew and system curl installations

## Prerequisites

- macOS with curl (included by default)
- Optional: Homebrew for additional curl features

## Build

```sh
cd gidinet-dns-client
make
```

This produces the `gidinet` executable.

## Usage

### Update an existing DNS record:
```sh
./gidinet update --username USER --passwordB64 PASS_B64 \
  --oldDomain example.com --oldHost test --oldType A --oldData 1.2.3.4 --oldTTL 300 --oldPriority 0 \
  --newDomain example.com --newHost test --newType A --newData 5.6.7.8 --newTTL 300 --newPriority 0
```

### Add a new DNS record:
```sh
./gidinet add --username USER --passwordB64 PASS_B64 \
  --domain example.com --host new --type A --data 9.8.7.6 --ttl 300 --priority 0
```

### Delete a DNS record:
```sh
./gidinet delete --username USER --passwordB64 PASS_B64 \
  --domain example.com --host test --type A --data 1.2.3.4 --ttl 300 --priority 0
```

### List DNS records:
```sh
./gidinet list --username USER --passwordB64 PASS_B64 --domain example.com
```

### Get version information:
```sh
./gidinet version     # or --version or -v
```

### Get help:
```sh
./gidinet                    # General help
./gidinet <command> --help   # Command-specific help
```

## JSON Output & Automation

All commands output clean JSON suitable for automation and scripting:

### Simple Operations (add/delete/update):
```json
{"result":{"code":0,"message":"Operation successful","subCode":0,"text":"Ok"}}
```

### List Operation:
```json
{"result":{"code":0,"message":"Operation successful","subCode":0},"records":[{"domain":"example.com","host":"test","type":"A","data":"1.2.3.4","ttl":300,"priority":0}],"recordCount":"1"}
```

### Using with jq:
```sh
# Get just the result message
./gidinet add --username USER --passwordB64 PASS_B64 --domain example.com --host test --type A --data 1.2.3.4 --ttl 300 --priority 0 | jq '.result.message'

# List all A records
./gidinet list --username USER --passwordB64 PASS_B64 --domain example.com | jq '.records[] | select(.type=="A")'

# Check if operation was successful
./gidinet delete --username USER --passwordB64 PASS_B64 --domain example.com --host test --type A --data 1.2.3.4 --ttl 300 --priority 0 | jq '.result.code == 0'
```

## Result Codes

The client translates numeric result codes to human-readable messages:

- **0**: Operation successful
- **1**: Authentication failed
- **2**: Operation failed - cannot modify read-only value
- **3**: Operation failed - invalid parameters
- **4**: Operation failed - undefined error
- **5**: Operation failed - object not found
- **6**: Operation failed - object in use

## Notes

- **JSON Output**: All operations return JSON for easy automation and scripting
- **Error Handling**: Error messages go to stderr, keeping JSON output clean
- **Security**: Uses full SSL/TLS verification for production-ready communication
- **Base64 Password**: The `--passwordB64` parameter expects a base64-encoded password
- **Priority**: Set to 0 for non-MX record types
- **jq Compatible**: No parsing errors when piping to jq or similar tools

## Development

```sh
make help    # Show available targets
make test    # Show usage examples
make clean   # Remove built files
make strip   # Strip symbols from existing binary
```

The build process automatically strips symbols for optimized binary size.

## Installation

```sh
make install    # Install to /usr/local/bin
```
