// DIGINET DNS API Client using libcurl
// Manual SOAP XML construction for exact format compatibility
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define VERSION "v0.1"

struct APIResponse {
    char *data;
    size_t size;
};

// Function to translate result codes to human-readable messages
const char* get_result_code_message(int result_code) {
    switch (result_code) {
        case 0: return "Operation successful";
        case 1: return "Authentication failed";
        case 2: return "Operation failed - cannot modify read-only value";
        case 3: return "Operation failed - invalid parameters";
        case 4: return "Operation failed - undefined error";
        case 5: return "Operation failed - object not found";
        case 6: return "Operation failed - object in use";
        default: return "Unknown result code";
    }
}

// Function to decode result sub code flags (basic implementation)
void print_result_subcode_info(int sub_code) {
    if (sub_code == 0) {
        return; // No additional information
    }
    
    printf("Additional error details (sub-code %d):\n", sub_code);
    
    // Common DNS-related bit flags (these would need to be specific per API operation)
    if (sub_code & (1 << 0)) printf("  - Bit 0: Domain validation issue\n");
    if (sub_code & (1 << 1)) printf("  - Bit 1: Host validation issue\n");
    if (sub_code & (1 << 2)) printf("  - Bit 2: Record type validation issue\n");
    if (sub_code & (1 << 3)) printf("  - Bit 3: Data validation issue\n");
    if (sub_code & (1 << 4)) printf("  - Bit 4: TTL validation issue\n");
    if (sub_code & (1 << 5)) printf("  - Bit 5: Priority validation issue\n");
    // Add more specific mappings as needed per API operation
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct APIResponse *response = (struct APIResponse *)userp;
    
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;
    
    return realsize;
}

// Helper function to extract text between XML tags
char* extract_xml_value(const char *xml, const char *tag) {
    char open_tag[256], close_tag[256];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);
    
    char *start = strstr(xml, open_tag);
    if (!start) return NULL;
    
    start += strlen(open_tag);
    char *end = strstr(start, close_tag);
    if (!end) return NULL;
    
    size_t len = end - start;
    char *value = malloc(len + 1);
    if (!value) return NULL;
    
    strncpy(value, start, len);
    value[len] = '\0';
    return value;
}

// Helper function to escape JSON strings
void print_json_string(const char *str) {
    if (!str) return;
    
    printf("\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':
                printf("\\\"");
                break;
            case '\\':
                printf("\\\\");
                break;
            case '\b':
                printf("\\b");
                break;
            case '\f':
                printf("\\f");
                break;
            case '\n':
                printf("\\n");
                break;
            case '\r':
                printf("\\r");
                break;
            case '\t':
                printf("\\t");
                break;
            default:
                if (*p < 0x20) {
                    printf("\\u%04x", (unsigned char)*p);
                } else {
                    printf("%c", *p);
                }
                break;
        }
    }
    printf("\"");
}

void parse_and_display_list_result(const char *response_data) {
    if (!response_data) {
        printf("{\"error\": \"No response data to parse\"}\n");
        return;
    }
    
    // Extract basic result information
    int result_code = -1;
    char *result_start = strstr(response_data, "<resultCode>");
    if (result_start) {
        result_start += 12;
        result_code = atoi(result_start);
    }
    
    int result_subcode = 0;
    char *subcode_start = strstr(response_data, "<resultSubCode>");
    if (subcode_start) {
        subcode_start += 15;
        result_subcode = atoi(subcode_start);
    }
    
    char *result_text = extract_xml_value(response_data, "resultText");
    
    // Start JSON output
    printf("{");
    printf("\"result\":{");
    printf("\"code\":%d,", result_code);
    printf("\"message\":");
    print_json_string(get_result_code_message(result_code));
    printf(",\"subCode\":%d", result_subcode);
    if (result_text) {
        printf(",\"text\":");
        print_json_string(result_text);
        free(result_text);
    }
    printf("}");
    
    // If successful, parse and display records
    if (result_code == 0) {
        printf(",\"records\":[");
        
        // Find resultItems section
        char *items_start = strstr(response_data, "<resultItems>");
        char *items_end = strstr(response_data, "</resultItems>");
        
        if (items_start && items_end) {
            items_start += 13; // length of "<resultItems>"
            
            // Parse each DNSRecordListItem
            char *current = items_start;
            int first_record = 1;
            
            while (current < items_end) {
                char *record_start = strstr(current, "<DNSRecordListItem>");
                if (!record_start || record_start >= items_end) break;
                
                char *record_end = strstr(record_start, "</DNSRecordListItem>");
                if (!record_end || record_end >= items_end) break;
                
                record_start += 19; // length of "<DNSRecordListItem>"
                
                // Extract record fields
                char record_xml[4096];
                size_t record_len = record_end - record_start;
                if (record_len < sizeof(record_xml)) {
                    strncpy(record_xml, record_start, record_len);
                    record_xml[record_len] = '\0';
                    
                    char *domain = extract_xml_value(record_xml, "DomainName");
                    char *host = extract_xml_value(record_xml, "HostName");
                    char *type = extract_xml_value(record_xml, "RecordType");
                    char *data = extract_xml_value(record_xml, "Data");
                    char *ttl_str = extract_xml_value(record_xml, "TTL");
                    char *priority_str = extract_xml_value(record_xml, "Priority");
                    char *readonly_str = extract_xml_value(record_xml, "ReadOnly");
                    char *suspended_str = extract_xml_value(record_xml, "Suspended");
                    char *suspension_reason = extract_xml_value(record_xml, "SuspensionReason");
                    
                    if (!first_record) printf(",");
                    first_record = 0;
                    
                    printf("{");
                    if (domain) {
                        printf("\"domain\":");
                        print_json_string(domain);
                        printf(",");
                    }
                    if (host) {
                        printf("\"host\":");
                        print_json_string(host);
                        printf(",");
                    }
                    if (type) {
                        printf("\"type\":");
                        print_json_string(type);
                        printf(",");
                    }
                    if (data) {
                        printf("\"data\":");
                        print_json_string(data);
                        printf(",");
                    }
                    if (ttl_str) printf("\"ttl\":%s,", ttl_str);
                    if (priority_str) printf("\"priority\":%s,", priority_str);
                    if (readonly_str) printf("\"readOnly\":%s,", 
                        (strcmp(readonly_str, "true") == 0) ? "true" : "false");
                    if (suspended_str) printf("\"suspended\":%s", 
                        (strcmp(suspended_str, "true") == 0) ? "true" : "false");
                    if (suspension_reason && strlen(suspension_reason) > 0) {
                        printf(",\"suspensionReason\":");
                        print_json_string(suspension_reason);
                    }
                    printf("}");
                    
                    // Free allocated memory
                    free(domain);
                    free(host);
                    free(type);
                    free(data);
                    free(ttl_str);
                    free(priority_str);
                    free(readonly_str);
                    free(suspended_str);
                    free(suspension_reason);
                }
                
                current = record_end + 20; // Move past </DNSRecordListItem>
            }
        }
        
        printf("]");
        
        // Add record count if available
        char *count_str = extract_xml_value(response_data, "resultItemCount");
        if (count_str) {
            printf(",\"recordCount\":%s", count_str);
            free(count_str);
        }
    }
    
    printf("}\n");
}

void parse_and_display_simple_result(const char *response_data) {
    if (!response_data) {
        printf("{\"error\":\"No response data to parse\"}\n");
        return;
    }
    
    // Extract resultCode
    int result_code = -1;
    char *result_start = strstr(response_data, "<resultCode>");
    if (result_start) {
        result_start += 12; // length of "<resultCode>"
        result_code = atoi(result_start);
    }
    
    // Extract resultSubCode
    int result_subcode = 0;
    char *subcode_start = strstr(response_data, "<resultSubCode>");
    if (subcode_start) {
        subcode_start += 15; // length of "<resultSubCode>"
        result_subcode = atoi(subcode_start);
    }
    
    // Extract resultText
    char *result_text = NULL;
    char *text_start = strstr(response_data, "<resultText>");
    char *text_end = strstr(response_data, "</resultText>");
    if (text_start && text_end) {
        text_start += 12; // length of "<resultText>"
        int text_len = text_end - text_start;
        result_text = malloc(text_len + 1);
        if (result_text) {
            strncpy(result_text, text_start, text_len);
            result_text[text_len] = '\0';
        }
    }
    
    // Output JSON
    printf("{");
    printf("\"result\":{");
    printf("\"code\":%d,", result_code);
    printf("\"message\":");
    print_json_string(get_result_code_message(result_code));
    printf(",\"subCode\":%d", result_subcode);
    if (result_text) {
        printf(",\"text\":");
        print_json_string(result_text);
    }
    printf("}");
    printf("}\n");
    
    if (result_text) free(result_text);
}

void parse_and_display_result(const char *response_data) {
    if (!response_data) {
        printf("No response data to parse.\n");
        return;
    }
    
    // Extract resultCode
    int result_code = -1;
    char *result_start = strstr(response_data, "<resultCode>");
    if (result_start) {
        result_start += 12; // length of "<resultCode>"
        result_code = atoi(result_start);
    }
    
    // Extract resultSubCode
    int result_subcode = 0;
    char *subcode_start = strstr(response_data, "<resultSubCode>");
    if (subcode_start) {
        subcode_start += 15; // length of "<resultSubCode>"
        result_subcode = atoi(subcode_start);
    }
    
    // Extract resultText
    char *text_start = strstr(response_data, "<resultText>");
    char *text_end = strstr(response_data, "</resultText>");
    
    printf("\n=== API Result ===\n");
    printf("Result Code: %d - %s\n", result_code, get_result_code_message(result_code));
    
    if (text_start && text_end) {
        text_start += 12; // length of "<resultText>"
        printf("Result Text: %.*s\n", (int)(text_end - text_start), text_start);
    }
    
    if (result_subcode > 0) {
        print_result_subcode_info(result_subcode);
    }
    
    printf("==================\n\n");
}

int call_record_update(const char *username, const char *passwordB64,
                       const char *oldDomain, const char *oldHost, const char *oldType, 
                       const char *oldData, int oldTTL, int oldPriority,
                       const char *newDomain, const char *newHost, const char *newType, 
                       const char *newData, int newTTL, int newPriority) {
    
    CURL *curl;
    CURLcode res;
    struct APIResponse response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        printf("Failed to initialize CURL\n");
        return 1;
    }
    
    // Construct the exact XML format that works
    char *xml_request;
    asprintf(&xml_request,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
        "xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
        " <soap:Body>\n"
        "  <recordUpdate xmlns=\"https://api.quickservicebox.com/DNS/DNSAPI\">\n"
        "   <accountUsername>%s</accountUsername>\n"
        "   <accountPasswordB64>%s</accountPasswordB64>\n"
        "   <oldRecord>\n"
        "    <DomainName>%s</DomainName>\n"
        "    <HostName>%s</HostName>\n"
        "    <RecordType>%s</RecordType>\n"
        "    <Data>%s</Data>\n"
        "    <TTL>%d</TTL>\n"
        "    <Priority>%d</Priority>\n"
        "   </oldRecord>\n"
        "   <newRecord>\n"
        "    <DomainName>%s</DomainName>\n"
        "    <HostName>%s</HostName>\n"
        "    <RecordType>%s</RecordType>\n"
        "    <Data>%s</Data>\n"
        "    <TTL>%d</TTL>\n"
        "    <Priority>%d</Priority>\n"
        "   </newRecord>\n"
        "  </recordUpdate>\n"
        " </soap:Body>\n"
        "</soap:Envelope>",
        username, passwordB64,
        oldDomain, oldHost, oldType, oldData ? oldData : "", oldTTL, oldPriority,
        newDomain, newHost, newType, newData, newTTL, newPriority);
    
    // Set up the request
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.quickservicebox.com/API/Beta/DNSAPI.asmx");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml_request);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(xml_request));
    
    // Set headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
    headers = curl_slist_append(headers, "SOAPAction: https://api.quickservicebox.com/DNS/DNSAPI/recordUpdate");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set up response handling
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Perform the request
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        printf("CURL request failed: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(xml_request);
        if (response.data) free(response.data);
        return 1;
    }
    
    // Parse and display result with human-readable messages
    parse_and_display_simple_result(response.data);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(xml_request);
    if (response.data) free(response.data);
    
    return 0;
}

int call_record_add(const char *username, const char *passwordB64,
                   const char *domain, const char *host, const char *type, 
                   const char *data, int ttl, int priority) {
    
    CURL *curl;
    CURLcode res;
    struct APIResponse response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return 1;
    }
    
    // Construct XML for recordAdd
    char *xml_request;
    asprintf(&xml_request,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
        "xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
        "<soap:Body>\n"
        "<recordAdd xmlns=\"https://api.quickservicebox.com/DNS/DNSAPI\">\n"
        "<accountUsername>%s</accountUsername>\n"
        "<accountPasswordB64>%s</accountPasswordB64>\n"
        "<record>\n"
        "<DomainName>%s</DomainName>\n"
        "<HostName>%s</HostName>\n"
        "<RecordType>%s</RecordType>\n"
        "<Data>%s</Data>\n"
        "<TTL>%d</TTL>\n"
        "<Priority>%d</Priority>\n"
        "</record>\n"
        "</recordAdd>\n"
        "</soap:Body>\n"
        "</soap:Envelope>",
        username, passwordB64, domain, host, type, data, ttl, priority);
    
    // Setup headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
    headers = curl_slist_append(headers, "SOAPAction: \"https://api.quickservicebox.com/DNS/DNSAPI/recordAdd\"");
    
    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.quickservicebox.com/API/Beta/DNSAPI.asmx");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml_request);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Request failed: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(xml_request);
        if (response.data) free(response.data);
        return 1;
    }
    
    // Process response with JSON output
    parse_and_display_simple_result(response.data);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(xml_request);
    if (response.data) free(response.data);
    
    return 0;
}

int call_record_delete(const char *username, const char *passwordB64,
                      const char *domain, const char *host, const char *type, 
                      const char *data, int ttl, int priority) {
    
    CURL *curl;
    CURLcode res;
    struct APIResponse response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return 1;
    }
    
    // Construct XML for recordDelete
    char *xml_request;
    asprintf(&xml_request,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
        "xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
        "<soap:Body>\n"
        "<recordDelete xmlns=\"https://api.quickservicebox.com/DNS/DNSAPI\">\n"
        "<accountUsername>%s</accountUsername>\n"
        "<accountPasswordB64>%s</accountPasswordB64>\n"
        "<record>\n"
        "<DomainName>%s</DomainName>\n"
        "<HostName>%s</HostName>\n"
        "<RecordType>%s</RecordType>\n"
        "<Data>%s</Data>\n"
        "<TTL>%d</TTL>\n"
        "<Priority>%d</Priority>\n"
        "</record>\n"
        "</recordDelete>\n"
        "</soap:Body>\n"
        "</soap:Envelope>",
        username, passwordB64, domain, host, type, data, ttl, priority);
    
    // Setup headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
    headers = curl_slist_append(headers, "SOAPAction: \"https://api.quickservicebox.com/DNS/DNSAPI/recordDelete\"");
    
    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.quickservicebox.com/API/Beta/DNSAPI.asmx");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml_request);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Request failed: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(xml_request);
        if (response.data) free(response.data);
        return 1;
    }
    
    // Process response with JSON output
    parse_and_display_simple_result(response.data);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(xml_request);
    if (response.data) free(response.data);
    
    return 0;
}

int call_record_list(const char *username, const char *passwordB64, const char *domain) {
    
    CURL *curl;
    CURLcode res;
    struct APIResponse response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        printf("Failed to initialize CURL\n");
        return 1;
    }
    
    // Construct XML for recordGetList
    char *xml_request;
    asprintf(&xml_request,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
        "xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
        "<soap:Body>\n"
        "<recordGetList xmlns=\"https://api.quickservicebox.com/DNS/DNSAPI\">\n"
        "<accountUsername>%s</accountUsername>\n"
        "<accountPasswordB64>%s</accountPasswordB64>\n"
        "<domainName>%s</domainName>\n"
        "</recordGetList>\n"
        "</soap:Body>\n"
        "</soap:Envelope>",
        username, passwordB64, domain);
    
    // Setup headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
    headers = curl_slist_append(headers, "SOAPAction: \"https://api.quickservicebox.com/DNS/DNSAPI/recordGetList\"");
    
    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.quickservicebox.com/API/Beta/DNSAPI.asmx");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml_request);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("Request failed: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(xml_request);
        if (response.data) free(response.data);
        return 1;
    }
    
    // Process response - output JSON format for list command
    parse_and_display_list_result(response.data);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(xml_request);
    if (response.data) free(response.data);
    
    return 0;
}

void print_usage(const char *prog) {
    printf("DIGINET DNS API Client %s - QuickServiceBox DNS Management\n\n", VERSION);
    printf("Usage: %s <command> [options]\n\n", prog);
    printf("Commands:\n");
    printf("  update    Update an existing DNS record\n");
    printf("  add       Add a new DNS record\n");
    printf("  delete    Delete an existing DNS record\n");
    printf("  list      List DNS records for a domain\n");
    printf("  version   Show version information\n\n");
    printf("Global options (required for all commands):\n");
    printf("  --username USER       API username\n");
    printf("  --passwordB64 PASS    API password (base64 encoded)\n\n");
    printf("For specific command usage, run: %s <command> --help\n\n", prog);
}

void print_update_usage(const char *prog) {
    printf("Usage: %s update [options]\n\n", prog);
    printf("Update an existing DNS record by specifying both old and new values.\n\n");
    printf("Required options:\n");
    printf("  --username USER       API username\n");
    printf("  --passwordB64 PASS    API password (base64 encoded)\n");
    printf("  --oldDomain DOMAIN    Current domain name\n");
    printf("  --oldHost HOST        Current hostname\n");
    printf("  --oldType TYPE        Current record type (A, AAAA, CNAME, MX, TXT, etc.)\n");
    printf("  --oldData DATA        Current record data\n");
    printf("  --oldTTL TTL          Current TTL in seconds\n");
    printf("  --oldPriority NUM     Current priority (0 for non-MX records)\n");
    printf("  --newDomain DOMAIN    New domain name\n");
    printf("  --newHost HOST        New hostname\n");
    printf("  --newType TYPE        New record type\n");
    printf("  --newData DATA        New record data\n");
    printf("  --newTTL TTL          New TTL in seconds\n");
    printf("  --newPriority NUM     New priority (0 for non-MX records)\n\n");
}

void print_add_usage(const char *prog) {
    printf("Usage: %s add [options]\n\n", prog);
    printf("Add a new DNS record.\n\n");
    printf("Required options:\n");
    printf("  --username USER       API username\n");
    printf("  --passwordB64 PASS    API password (base64 encoded)\n");
    printf("  --domain DOMAIN       Domain name\n");
    printf("  --host HOST           Hostname\n");
    printf("  --type TYPE           Record type (A, AAAA, CNAME, MX, TXT, etc.)\n");
    printf("  --data DATA           Record data\n");
    printf("  --ttl TTL             TTL in seconds\n");
    printf("  --priority NUM        Priority (0 for non-MX records)\n\n");
}

void print_delete_usage(const char *prog) {
    printf("Usage: %s delete [options]\n\n", prog);
    printf("Delete an existing DNS record.\n\n");
    printf("Required options:\n");
    printf("  --username USER       API username\n");
    printf("  --passwordB64 PASS    API password (base64 encoded)\n");
    printf("  --domain DOMAIN       Domain name\n");
    printf("  --host HOST           Hostname\n");
    printf("  --type TYPE           Record type (A, AAAA, CNAME, MX, TXT, etc.)\n");
    printf("  --data DATA           Record data\n");
    printf("  --ttl TTL             TTL in seconds\n");
    printf("  --priority NUM        Priority (0 for non-MX records)\n\n");
}

void print_list_usage(const char *prog) {
    printf("Usage: %s list [options]\n\n", prog);
    printf("List DNS records for a domain.\n\n");
    printf("Required options:\n");
    printf("  --username USER       API username\n");
    printf("  --passwordB64 PASS    API password (base64 encoded)\n");
    printf("  --domain DOMAIN       Domain name to list records for\n\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *command = argv[1];
    
    // Handle help for specific commands
    if (argc == 3 && strcmp(argv[2], "--help") == 0) {
        if (strcmp(command, "update") == 0) {
            print_update_usage(argv[0]);
        } else if (strcmp(command, "add") == 0) {
            print_add_usage(argv[0]);
        } else if (strcmp(command, "delete") == 0) {
            print_delete_usage(argv[0]);
        } else if (strcmp(command, "list") == 0) {
            print_list_usage(argv[0]);
        } else {
            printf("Unknown command: %s\n", command);
            print_usage(argv[0]);
            return 1;
        }
        return 0;
    }
    
    // Parse common parameters
    char *username = NULL, *passwordB64 = NULL;
    
    // Command-specific parameters
    char *domain = NULL, *host = NULL, *type = NULL, *data = NULL;
    int ttl = 0, priority = 0;
    
    // Update-specific parameters (old values)
    char *oldDomain = NULL, *oldHost = NULL, *oldType = NULL, *oldData = NULL;
    int oldTTL = 0, oldPriority = 0;
    char *newDomain = NULL, *newHost = NULL, *newType = NULL, *newData = NULL;
    int newTTL = 0, newPriority = 0;
    
    // Parse command line arguments starting from index 2 (after command)
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--username") == 0 && i + 1 < argc) username = argv[++i];
        else if (strcmp(argv[i], "--passwordB64") == 0 && i + 1 < argc) passwordB64 = argv[++i];
        else if (strcmp(argv[i], "--domain") == 0 && i + 1 < argc) domain = argv[++i];
        else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) host = argv[++i];
        else if (strcmp(argv[i], "--type") == 0 && i + 1 < argc) type = argv[++i];
        else if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) data = argv[++i];
        else if (strcmp(argv[i], "--ttl") == 0 && i + 1 < argc) ttl = atoi(argv[++i]);
        else if (strcmp(argv[i], "--priority") == 0 && i + 1 < argc) priority = atoi(argv[++i]);
        // Update command specific parameters
        else if (strcmp(argv[i], "--oldDomain") == 0 && i + 1 < argc) oldDomain = argv[++i];
        else if (strcmp(argv[i], "--oldHost") == 0 && i + 1 < argc) oldHost = argv[++i];
        else if (strcmp(argv[i], "--oldType") == 0 && i + 1 < argc) oldType = argv[++i];
        else if (strcmp(argv[i], "--oldData") == 0 && i + 1 < argc) oldData = argv[++i];
        else if (strcmp(argv[i], "--oldTTL") == 0 && i + 1 < argc) oldTTL = atoi(argv[++i]);
        else if (strcmp(argv[i], "--oldPriority") == 0 && i + 1 < argc) oldPriority = atoi(argv[++i]);
        else if (strcmp(argv[i], "--newDomain") == 0 && i + 1 < argc) newDomain = argv[++i];
        else if (strcmp(argv[i], "--newHost") == 0 && i + 1 < argc) newHost = argv[++i];
        else if (strcmp(argv[i], "--newType") == 0 && i + 1 < argc) newType = argv[++i];
        else if (strcmp(argv[i], "--newData") == 0 && i + 1 < argc) newData = argv[++i];
        else if (strcmp(argv[i], "--newTTL") == 0 && i + 1 < argc) newTTL = atoi(argv[++i]);
        else if (strcmp(argv[i], "--newPriority") == 0 && i + 1 < argc) newPriority = atoi(argv[++i]);
        else {
            printf("Unknown option: %s\n", argv[i]);
            return 1;
        }
    }
    
    // Validate command and required parameters
    if (strcmp(command, "update") == 0) {
        if (!username || !passwordB64 || !oldDomain || !oldHost || !oldType || !oldData ||
            !newDomain || !newHost || !newType || !newData) {
            printf("Error: Missing required parameters for update command.\n\n");
            print_update_usage(argv[0]);
            return 1;
        }
        return call_record_update(username, passwordB64, oldDomain, oldHost, oldType, oldData, oldTTL, oldPriority,
                                 newDomain, newHost, newType, newData, newTTL, newPriority);
    } else if (strcmp(command, "add") == 0) {
        if (!username || !passwordB64 || !domain || !host || !type || !data) {
            printf("Error: Missing required parameters for add command.\n\n");
            print_add_usage(argv[0]);
            return 1;
        }
        return call_record_add(username, passwordB64, domain, host, type, data, ttl, priority);
    } else if (strcmp(command, "delete") == 0) {
        if (!username || !passwordB64 || !domain || !host || !type || !data) {
            printf("Error: Missing required parameters for delete command.\n\n");
            print_delete_usage(argv[0]);
            return 1;
        }
        return call_record_delete(username, passwordB64, domain, host, type, data, ttl, priority);
    } else if (strcmp(command, "list") == 0) {
        if (!username || !passwordB64 || !domain) {
            printf("Error: Missing required parameters for list command.\n\n");
            print_list_usage(argv[0]);
            return 1;
        }
        return call_record_list(username, passwordB64, domain);
    } else if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
        printf("DIGINET DNS API Client %s\n", VERSION);
        return 0;
    } else {
        printf("Unknown command: %s\n", command);
        print_usage(argv[0]);
        return 1;
    }
}