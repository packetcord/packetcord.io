#include <cord_flow/table/cord_lpm.h>
#include <cord_error.h>
#include <cord_retval.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

//
// Test Result Tracking
//

typedef struct
{
    int total;
    int passed;
    int failed;
} test_results_t;

static test_results_t results = {0, 0, 0};

#define TEST_ASSERT(condition, msg) do { \
    results.total++; \
    if (condition) { \
        results.passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        results.failed++; \
        printf("  [FAIL] %s\n", msg); \
    } \
} while(0)

//
// IPv4 LPM Tests
//

void test_ipv4_lpm(void)
{
    printf("\n=== IPv4 LPM Tests ===\n");

    // Create LPM table
    cord_ipv4_lpm_t *lpm = cord_ipv4_lpm_create(100);
    TEST_ASSERT(lpm != NULL, "IPv4 LPM table created");

    // Test entries: mix of different prefix lengths
    struct {
        const char *cidr;
        uint32_t next_hop;
    } test_routes[] = {
        // Default route
        {"0.0.0.0/0", 1},

        // Class A ranges (different from before)
        {"11.0.0.0/8", 15},
        {"11.22.0.0/16", 16},
        {"11.22.33.0/24", 17},
        {"11.22.33.64/26", 18},

        // Class B ranges
        {"172.20.0.0/16", 25},
        {"172.20.5.0/24", 26},
        {"172.20.5.192/27", 27},

        // Class C ranges
        {"192.168.100.0/24", 35},
        {"192.168.100.0/25", 36},
        {"192.168.100.128/26", 37},
        {"192.168.100.192/27", 38},

        // Public IP ranges
        {"8.8.4.0/24", 45},
        {"9.9.9.0/24", 46},
        {"1.0.0.1/32", 47},

        // Deep prefixes requiring TBL8
        {"198.51.100.0/24", 55},
        {"198.51.100.128/25", 56},
        {"198.51.100.192/26", 57},
        {"198.51.100.224/27", 58},
        {"198.51.100.240/28", 59}
    };

    // Add all routes
    printf("\nAdding 20 IPv4 routes...\n");
    for (size_t i = 0; i < sizeof(test_routes) / sizeof(test_routes[0]); i++)
    {
        uint32_t ip;
        uint8_t depth;

        int ret = cord_ipv4_parse_cidr(test_routes[i].cidr, &ip, &depth);
        TEST_ASSERT(ret == 0, test_routes[i].cidr);

        ret = cord_ipv4_lpm_add(lpm, ip, depth, test_routes[i].next_hop);
        TEST_ASSERT(ret == 0, "Route added successfully");
    }

    // Test lookups - exact matches
    printf("\nTesting exact lookups...\n");
    struct {
        const char *ip_str;
        uint32_t expected_next_hop;
    } test_lookups[] = {
        // Most specific matches
        {"11.22.33.80", 18},         // Matches 11.22.33.64/26 (64-127)
        {"11.22.33.10", 17},         // Matches 11.22.33.0/24
        {"11.22.50.1", 16},          // Matches 11.22.0.0/16
        {"11.50.0.1", 15},           // Matches 11.0.0.0/8
        {"172.20.5.200", 27},        // Matches 172.20.5.192/27 (192-223)
        {"192.168.100.50", 36},      // Matches 192.168.100.0/25 (0-127)
        {"192.168.100.150", 37},     // Matches 192.168.100.128/26 (128-191)
        {"1.0.0.1", 47},             // Exact /32 match
        {"198.51.100.245", 59},      // Matches 198.51.100.240/28
        {"198.51.100.230", 58},      // Matches 198.51.100.224/27
        {"5.5.5.5", 1},              // Matches default route
    };

    for (size_t i = 0; i < sizeof(test_lookups) / sizeof(test_lookups[0]); i++)
    {
        uint32_t ip;
        cord_ipv4_str_to_binary(test_lookups[i].ip_str, &ip);

        uint32_t next_hop = cord_ipv4_lpm_lookup(lpm, ip);
        char msg[128];
        snprintf(msg, sizeof(msg), "%s -> next_hop %u (expected %u)",
                 test_lookups[i].ip_str, next_hop, test_lookups[i].expected_next_hop);
        TEST_ASSERT(next_hop == test_lookups[i].expected_next_hop, msg);
    }

    // Test deletion
    printf("\nTesting route deletion...\n");
    uint32_t ip;
    uint8_t depth;
    cord_ipv4_parse_cidr("11.22.33.64/26", &ip, &depth);
    int ret = cord_ipv4_lpm_delete(lpm, ip, depth);
    TEST_ASSERT(ret == 0, "Route deleted successfully");

    // Verify deletion - should now match less specific route
    cord_ipv4_str_to_binary("11.22.33.80", &ip);
    uint32_t next_hop = cord_ipv4_lpm_lookup(lpm, ip);
    TEST_ASSERT(next_hop == 17, "After deletion, matches parent route");

    // Print statistics
    printf("\n");
    cord_ipv4_lpm_print_stats(lpm);

    // Cleanup
    cord_ipv4_lpm_destroy(lpm);
}

//
// IPv6 LPM Tests
//

void test_ipv6_lpm(void)
{
    printf("\n=== IPv6 LPM Tests ===\n");

    // Create LPM table
    cord_ipv6_lpm_t *lpm = cord_ipv6_lpm_create(100);
    TEST_ASSERT(lpm != NULL, "IPv6 LPM table created");

    // Test entries: realistic IPv6 prefixes
    struct {
        const char *cidr;
        uint32_t next_hop;
    } test_routes[] = {
        // Default route
        {"::/0", 1},

        // Documentation ranges (RFC 3849) - different hierarchy
        {"2001:db8::/32", 110},
        {"2001:db8:a::/48", 111},
        {"2001:db8:a:b::/64", 112},
        {"2001:db8:a:b:c::/80", 113},

        // Global unicast (2000::/3) - different providers
        {"2002::/16", 210},
        {"2400:cb00::/32", 211},        // Cloudflare
        {"2400:cb00:2048::/48", 212},   // Cloudflare specific

        // Link-local (fe80::/10)
        {"fe80::/10", 310},
        {"fe80::/64", 311},

        // Unique local (fc00::/7) - different ranges
        {"fc00::/7", 410},
        {"fd12::/16", 411},
        {"fd12:3456::/32", 412},
        {"fd12:3456:7890::/48", 413},

        // More specific test routes
        {"2607:f8b0::/32", 510},        // Google
        {"2607:f8b0:4000::/48", 511},   // Google specific
        {"2a01:111::/32", 610},         // Microsoft
        {"2a01:111:f400::/48", 611},
        {"2a01:111:f400:7c00::/56", 612}
    };

    // Add all routes
    printf("\nAdding 20 IPv6 routes...\n");
    for (size_t i = 0; i < sizeof(test_routes) / sizeof(test_routes[0]); i++)
    {
        cord_ipv6_addr_t ip;
        uint8_t depth;

        int ret = cord_ipv6_parse_cidr(test_routes[i].cidr, &ip, &depth);
        TEST_ASSERT(ret == 0, test_routes[i].cidr);

        ret = cord_ipv6_lpm_add(lpm, &ip, depth, test_routes[i].next_hop);
        TEST_ASSERT(ret == 0, "Route added successfully");
    }

    // Test lookups
    printf("\nTesting IPv6 lookups...\n");
    struct {
        const char *ip_str;
        uint32_t expected_next_hop;
    } test_lookups[] = {
        {"2001:db8:a:b:c::1", 113},              // Matches /80
        {"2001:db8:a:b:d::1", 112},              // Matches /64
        {"2001:db8:a:c::", 111},                 // Matches /48
        {"2001:db8:b::", 110},                   // Matches /32
        {"2400:cb00:2048::1", 212},              // Cloudflare specific
        {"fe80::1", 311},                        // Link-local /64
        {"fd12:3456:7890::1", 413},              // ULA /48
        {"2607:f8b0:4000::1", 511},              // Google specific
        {"2a01:111:f400:7c00::1", 612},          // Most specific
        {"3ffe:1234::", 1},                      // Default route
    };

    for (size_t i = 0; i < sizeof(test_lookups) / sizeof(test_lookups[0]); i++)
    {
        cord_ipv6_addr_t ip;
        cord_ipv6_str_to_binary(test_lookups[i].ip_str, &ip);

        uint32_t next_hop = cord_ipv6_lpm_lookup(lpm, &ip);
        char msg[256];
        snprintf(msg, sizeof(msg), "%s -> next_hop %u (expected %u)",
                 test_lookups[i].ip_str, next_hop, test_lookups[i].expected_next_hop);
        TEST_ASSERT(next_hop == test_lookups[i].expected_next_hop, msg);
    }

    // Test deletion
    printf("\nTesting IPv6 route deletion...\n");
    cord_ipv6_addr_t ip;
    uint8_t depth;
    cord_ipv6_parse_cidr("2001:db8:a:b:c::/80", &ip, &depth);
    int ret = cord_ipv6_lpm_delete(lpm, &ip, depth);
    TEST_ASSERT(ret == 0, "IPv6 route deleted successfully");

    // Verify deletion
    cord_ipv6_str_to_binary("2001:db8:a:b:c::1", &ip);
    uint32_t next_hop = cord_ipv6_lpm_lookup(lpm, &ip);
    CORD_LOG("next_hop of 2001:db8:a:b:c::1 is %u\n", next_hop);
    TEST_ASSERT(next_hop == 112, "After deletion, matches parent /64 route");

    // Print statistics
    printf("\n");
    cord_ipv6_lpm_print_stats(lpm);

    // Cleanup
    cord_ipv6_lpm_destroy(lpm);
}

//
// MAC Address Helper Tests
//

void test_mac_helpers(void)
{
    printf("\n=== MAC Address Helper Tests ===\n");

    // Test MAC to uint64_t conversion
    struct {
        const char *mac_str;
        uint64_t expected_u64;
    } test_conversions[] = {
        {"00:00:00:00:00:00", 0x000000000000ULL},
        {"ff:ff:ff:ff:ff:ff", 0xffffffffffffULL},
        {"ab:cd:ef:12:34:56", 0xabcdef123456ULL},
        {"12:34:56:ab:cd:ef", 0x123456abcdefULL},
        {"ca:fe:ba:be:f0:0d", 0xcafebabef00dULL},
        {"de:ad:c0:de:da:7a", 0xdeadc0deda7aULL},
        {"00:ff:00:ff:00:ff", 0x00ff00ff00ffULL},
        {"11:11:11:11:11:11", 0x111111111111ULL},
        {"88:88:88:88:88:88", 0x888888888888ULL},
        {"a1:b2:c3:d4:e5:f6", 0xa1b2c3d4e5f6ULL},
        {"98:76:54:32:10:fe", 0x9876543210feULL},
        {"00:a0:c9:14:c8:29", 0x00a0c914c829ULL},
        {"f8:e7:d6:c5:b4:a3", 0xf8e7d6c5b4a3ULL},
        {"08:00:20:0a:bc:de", 0x0800200abcdeULL},
        {"52:54:00:ab:cd:ef", 0x525400abcdefULL},
        {"00:50:56:11:22:33", 0x005056112233ULL},
        {"00:0c:29:aa:bb:cc", 0x000c29aabbccULL},
        {"00:16:3e:44:55:66", 0x00163e445566ULL},
        {"02:42:ac:1f:00:0a", 0x0242ac1f000aULL},
        {"fa:ce:b0:0c:c0:ff", 0xfaceb00cc0ffULL}
    };

    printf("\nTesting MAC to uint64_t conversion (20 entries)...\n");
    for (size_t i = 0; i < sizeof(test_conversions) / sizeof(test_conversions[0]); i++)
    {
        cord_mac_addr_t mac;
        int ret = cord_mac_str_to_binary(test_conversions[i].mac_str, &mac);
        TEST_ASSERT(ret == 0, "MAC string parsed");

        uint64_t mac_u64;
        cord_mac_to_u64(&mac, &mac_u64);

        char msg[128];
        snprintf(msg, sizeof(msg), "%s -> 0x%012lx (expected 0x%012lx)",
                 test_conversions[i].mac_str, mac_u64, test_conversions[i].expected_u64);
        TEST_ASSERT(mac_u64 == test_conversions[i].expected_u64, msg);
    }

    // Test uint64_t to MAC conversion (round-trip)
    printf("\nTesting uint64_t to MAC conversion (round-trip)...\n");
    for (size_t i = 0; i < sizeof(test_conversions) / sizeof(test_conversions[0]); i++)
    {
        cord_mac_addr_t mac;
        cord_u64_to_mac(test_conversions[i].expected_u64, &mac);

        char mac_str[18];
        cord_mac_to_str(&mac, mac_str, sizeof(mac_str));

        char msg[128];
        snprintf(msg, sizeof(msg), "0x%012lx -> %s (expected %s)",
                 test_conversions[i].expected_u64, mac_str, test_conversions[i].mac_str);
        TEST_ASSERT(strcasecmp(mac_str, test_conversions[i].mac_str) == 0, msg);
    }

    // Test MAC CIDR parsing
    printf("\nTesting MAC CIDR parsing...\n");
    struct {
        const char *cidr;
        uint8_t expected_depth;
    } cidr_tests[] = {
        {"00:00:00:00:00:00/0", 0},
        {"ab:cd:ef:12:34:56/24", 24},
        {"12:34:56:ab:cd:ef/48", 48},
    };

    for (size_t i = 0; i < sizeof(cidr_tests) / sizeof(cidr_tests[0]); i++)
    {
        cord_mac_addr_t mac;
        uint8_t depth;
        int ret = cord_mac_parse_cidr(cidr_tests[i].cidr, &mac, &depth);

        char msg[128];
        snprintf(msg, sizeof(msg), "%s parsed (depth=%u)", cidr_tests[i].cidr, depth);
        TEST_ASSERT(ret == 0 && depth == cidr_tests[i].expected_depth, msg);
    }
}

//
// Main Test Runner
//

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║          CORD-FLOW LPM Unit Test Suite                  ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    // Run all tests
    test_ipv4_lpm();
    test_ipv6_lpm();
    test_mac_helpers();

    // Print summary
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║                    Test Summary                          ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  Total:  %4d tests                                      ║\n", results.total);
    printf("║  Passed: %4d tests (%3d%%)                               ║\n",
           results.passed, results.total > 0 ? (results.passed * 100 / results.total) : 0);
    printf("║  Failed: %4d tests (%3d%%)                               ║\n",
           results.failed, results.total > 0 ? (results.failed * 100 / results.total) : 0);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");

    return (results.failed == 0) ? 0 : 1;
}
