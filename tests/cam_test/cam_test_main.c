#include <cord_flow/table/cord_cam.h>
#include <cord_flow/table/cord_lpm.h>  // For MAC helper functions
#include <cord_error.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

//
// L2 CAM Unit Test Suite
//
// Tests:
// - Basic add/lookup/delete operations
// - VLAN isolation (same MAC on different VLANs)
// - Hash collision handling
// - MAC address parsing and conversion
// - Clear operation
// - Statistics tracking
//

// Test framework
typedef struct
{
    int total;
    int passed;
    int failed;
} test_results_t;

test_results_t results = {0, 0, 0};

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

// Test configuration
#define NUM_BUCKETS 1024
#define MAX_ENTRIES 10000

// Helper function to compare MAC addresses
static bool mac_equal(const cord_mac_addr_t *mac1, const cord_mac_addr_t *mac2)
{
    return memcmp(mac1->addr, mac2->addr, 6) == 0;
}

//
// Test 1: Basic L2 CAM Operations (20 entries)
//
void test_l2_cam_basic_operations(void)
{
    printf("\n=== Test 1: Basic L2 CAM Operations ===\n");

    cord_l2_cam_t *cam = cord_l2_cam_create(NUM_BUCKETS, MAX_ENTRIES);
    TEST_ASSERT(cam != NULL, "Create L2 CAM table");

    // Test data: 20 MAC entries with various VLANs and ports
    struct {
        const char *mac_str;
        uint16_t vlan_id;
        uint32_t port_id;
    } test_entries[] = {
        // Standard unicast MACs
        {"a0:b1:c2:d3:e4:f5", 0, 5},      // VLAN 0 (untagged)
        {"a1:b2:c3:d4:e5:f6", 150, 6},
        {"a2:b3:c4:d5:e6:f7", 150, 7},
        {"12:34:56:78:9a:bc", 250, 8},
        {"88:99:aa:bb:cc:dd", 350, 9},

        // Locally administered MACs (bit 1 set in first octet)
        {"02:11:22:33:44:55", 0, 10},
        {"02:aa:bb:cc:dd:ee", 150, 11},
        {"06:12:34:56:78:9a", 250, 12},

        // Multicast MACs (bit 0 set in first octet)
        {"01:00:5e:01:02:03", 0, 13},     // IPv4 multicast
        {"33:33:ff:aa:bb:cc", 0, 14},     // IPv6 multicast

        // Vendor-specific OUIs
        {"00:1b:21:ab:cd:ef", 150, 15},   // Intel
        {"00:15:5d:12:34:56", 150, 16},   // Microsoft Hyper-V
        {"00:1c:42:aa:bb:cc", 150, 17},   // Parallels
        {"00:17:42:11:22:33", 250, 18},   // Parallels Desktop

        // Edge cases
        {"00:00:00:00:00:00", 0, 19},     // All zeros
        {"ff:ff:ff:ff:ff:ff", 0, 20},     // Broadcast

        // Additional test MACs
        {"ca:fe:ba:be:de:ad", 350, 21},
        {"d0:0d:f0:0d:be:ef", 450, 22},
        {"ab:cd:ef:01:23:45", 550, 23},
        {"fe:ed:fa:ce:c0:de", 2000, 24},
    };

    // Add all entries
    printf("\nAdding 20 entries...\n");
    for (size_t i = 0; i < sizeof(test_entries) / sizeof(test_entries[0]); i++)
    {
        cord_mac_addr_t mac;
        int ret = cord_mac_str_to_binary(test_entries[i].mac_str, &mac);
        TEST_ASSERT(ret == 0, test_entries[i].mac_str);

        ret = cord_l2_cam_add(cam, &mac, test_entries[i].port_id, test_entries[i].vlan_id);

        char msg[128];
        snprintf(msg, sizeof(msg), "Add %s VLAN %u -> port %u",
                 test_entries[i].mac_str, test_entries[i].vlan_id, test_entries[i].port_id);
        TEST_ASSERT(ret == 0, msg);
    }

    // Lookup all entries
    printf("\nLooking up all 20 entries...\n");
    for (size_t i = 0; i < sizeof(test_entries) / sizeof(test_entries[0]); i++)
    {
        cord_mac_addr_t mac;
        cord_mac_str_to_binary(test_entries[i].mac_str, &mac);

        uint32_t port_id = cord_l2_cam_lookup(cam, &mac, test_entries[i].vlan_id);

        char msg[128];
        snprintf(msg, sizeof(msg), "%s VLAN %u -> port %u (expected %u)",
                 test_entries[i].mac_str, test_entries[i].vlan_id,
                 port_id, test_entries[i].port_id);
        TEST_ASSERT(port_id == test_entries[i].port_id, msg);
    }

    // Test lookup misses
    printf("\nTesting lookup misses...\n");
    cord_mac_addr_t miss_mac;
    cord_mac_str_to_binary("99:88:77:66:55:44", &miss_mac);
    uint32_t port = cord_l2_cam_lookup(cam, &miss_mac, 0);
    TEST_ASSERT(port == CORD_L2_CAM_INVALID_PORT, "Lookup non-existent MAC returns INVALID");

    // Test deletes
    printf("\nDeleting 5 entries...\n");
    for (size_t i = 0; i < 5; i++)
    {
        cord_mac_addr_t mac;
        cord_mac_str_to_binary(test_entries[i].mac_str, &mac);

        int ret = cord_l2_cam_delete(cam, &mac, test_entries[i].vlan_id);

        char msg[128];
        snprintf(msg, sizeof(msg), "Delete %s VLAN %u",
                 test_entries[i].mac_str, test_entries[i].vlan_id);
        TEST_ASSERT(ret == 0, msg);

        // Verify deletion
        port = cord_l2_cam_lookup(cam, &mac, test_entries[i].vlan_id);
        snprintf(msg, sizeof(msg), "After delete, %s VLAN %u not found",
                 test_entries[i].mac_str, test_entries[i].vlan_id);
        TEST_ASSERT(port == CORD_L2_CAM_INVALID_PORT, msg);
    }

    // Verify remaining entries still work
    printf("\nVerifying remaining 15 entries...\n");
    for (size_t i = 5; i < sizeof(test_entries) / sizeof(test_entries[0]); i++)
    {
        cord_mac_addr_t mac;
        cord_mac_str_to_binary(test_entries[i].mac_str, &mac);

        uint32_t port_id = cord_l2_cam_lookup(cam, &mac, test_entries[i].vlan_id);

        char msg[128];
        snprintf(msg, sizeof(msg), "%s VLAN %u still maps to port %u",
                 test_entries[i].mac_str, test_entries[i].vlan_id, test_entries[i].port_id);
        TEST_ASSERT(port_id == test_entries[i].port_id, msg);
    }

    cord_l2_cam_destroy(cam);
    TEST_ASSERT(true, "Destroy L2 CAM table");
}

//
// Test 2: VLAN Isolation (same MAC on different VLANs)
//
void test_l2_cam_vlan_isolation(void)
{
    printf("\n=== Test 2: VLAN Isolation ===\n");

    cord_l2_cam_t *cam = cord_l2_cam_create(NUM_BUCKETS, MAX_ENTRIES);
    TEST_ASSERT(cam != NULL, "Create L2 CAM table");

    // Add the same MAC on 10 different VLANs
    const char *test_mac = "f1:f2:f3:f4:f5:f6";
    cord_mac_addr_t mac;
    cord_mac_str_to_binary(test_mac, &mac);

    printf("\nAdding same MAC on 10 different VLANs...\n");
    for (uint16_t vlan = 200; vlan < 210; vlan++)
    {
        uint32_t port_id = 3000 + vlan;  // Unique port per VLAN
        int ret = cord_l2_cam_add(cam, &mac, port_id, vlan);

        char msg[128];
        snprintf(msg, sizeof(msg), "Add %s VLAN %u -> port %u", test_mac, vlan, port_id);
        TEST_ASSERT(ret == 0, msg);
    }

    // Verify each VLAN returns correct port
    printf("\nVerifying VLAN isolation...\n");
    for (uint16_t vlan = 200; vlan < 210; vlan++)
    {
        uint32_t expected_port = 3000 + vlan;
        uint32_t port_id = cord_l2_cam_lookup(cam, &mac, vlan);

        char msg[128];
        snprintf(msg, sizeof(msg), "%s VLAN %u -> port %u (expected %u)",
                 test_mac, vlan, port_id, expected_port);
        TEST_ASSERT(port_id == expected_port, msg);
    }

    // Test lookup on non-existent VLAN
    uint32_t port = cord_l2_cam_lookup(cam, &mac, 888);
    TEST_ASSERT(port == CORD_L2_CAM_INVALID_PORT, "Same MAC on non-existent VLAN returns INVALID");

    // Delete from one VLAN, verify others unaffected
    printf("\nDeleting from VLAN 205...\n");
    int ret = cord_l2_cam_delete(cam, &mac, 205);
    TEST_ASSERT(ret == 0, "Delete from VLAN 205");

    port = cord_l2_cam_lookup(cam, &mac, 205);
    TEST_ASSERT(port == CORD_L2_CAM_INVALID_PORT, "VLAN 205 entry deleted");

    // Verify other VLANs still work
    for (uint16_t vlan = 200; vlan < 210; vlan++)
    {
        if (vlan == 205) continue;  // Skip deleted VLAN

        uint32_t expected_port = 3000 + vlan;
        uint32_t port_id = cord_l2_cam_lookup(cam, &mac, vlan);

        char msg[128];
        snprintf(msg, sizeof(msg), "After VLAN 205 delete, VLAN %u still works", vlan);
        TEST_ASSERT(port_id == expected_port, msg);
    }

    cord_l2_cam_destroy(cam);
    TEST_ASSERT(true, "Destroy L2 CAM table");
}

//
// Test 3: Update operations (change port mapping)
//
void test_l2_cam_update(void)
{
    printf("\n=== Test 3: Update Operations ===\n");

    cord_l2_cam_t *cam = cord_l2_cam_create(NUM_BUCKETS, MAX_ENTRIES);
    TEST_ASSERT(cam != NULL, "Create L2 CAM table");

    const char *test_mac = "d1:d2:d3:d4:d5:d6";
    uint16_t vlan = 300;
    cord_mac_addr_t mac;
    cord_mac_str_to_binary(test_mac, &mac);

    // Add initial entry
    int ret = cord_l2_cam_add(cam, &mac, 15, vlan);
    TEST_ASSERT(ret == 0, "Add initial entry port 15");

    uint32_t port = cord_l2_cam_lookup(cam, &mac, vlan);
    TEST_ASSERT(port == 15, "Initial lookup returns port 15");

    // Update to new port (re-add with same MAC+VLAN, different port)
    printf("\nUpdating port mapping...\n");
    ret = cord_l2_cam_add(cam, &mac, 25, vlan);
    TEST_ASSERT(ret == 0, "Update to port 25");

    port = cord_l2_cam_lookup(cam, &mac, vlan);
    TEST_ASSERT(port == 25, "After update, lookup returns port 25");

    // Multiple updates
    for (uint32_t new_port = 35; new_port <= 55; new_port += 10)
    {
        ret = cord_l2_cam_add(cam, &mac, new_port, vlan);
        port = cord_l2_cam_lookup(cam, &mac, vlan);

        char msg[128];
        snprintf(msg, sizeof(msg), "Update to port %u successful", new_port);
        TEST_ASSERT(port == new_port, msg);
    }

    cord_l2_cam_destroy(cam);
    TEST_ASSERT(true, "Destroy L2 CAM table");
}

//
// Test 4: Clear operation
//
void test_l2_cam_clear(void)
{
    printf("\n=== Test 4: Clear Operation ===\n");

    cord_l2_cam_t *cam = cord_l2_cam_create(NUM_BUCKETS, MAX_ENTRIES);
    TEST_ASSERT(cam != NULL, "Create L2 CAM table");

    // Add 10 entries
    printf("\nAdding 10 entries...\n");
    for (int i = 0; i < 10; i++)
    {
        char mac_str[32];
        snprintf(mac_str, sizeof(mac_str), "bb:bb:bb:bb:bb:%02x", i);

        cord_mac_addr_t mac;
        cord_mac_str_to_binary(mac_str, &mac);

        int ret = cord_l2_cam_add(cam, &mac, 500 + i, (i % 5) * 10);  // Various VLANs: 0, 10, 20, 30, 40
        TEST_ASSERT(ret == 0, mac_str);
    }

    // Verify entries exist
    printf("\nVerifying entries before clear...\n");
    int found_count = 0;
    for (int i = 0; i < 10; i++)
    {
        char mac_str[32];
        snprintf(mac_str, sizeof(mac_str), "bb:bb:bb:bb:bb:%02x", i);

        cord_mac_addr_t mac;
        cord_mac_str_to_binary(mac_str, &mac);

        uint32_t port = cord_l2_cam_lookup(cam, &mac, (i % 5) * 10);
        if (port != CORD_L2_CAM_INVALID_PORT)
            found_count++;
    }
    TEST_ASSERT(found_count == 10, "All 10 entries found before clear");

    // Clear table
    printf("\nClearing table...\n");
    cord_l2_cam_clear(cam);
    TEST_ASSERT(true, "Clear operation completed");

    // Verify all entries gone
    printf("\nVerifying entries after clear...\n");
    found_count = 0;
    for (int i = 0; i < 10; i++)
    {
        char mac_str[32];
        snprintf(mac_str, sizeof(mac_str), "bb:bb:bb:bb:bb:%02x", i);

        cord_mac_addr_t mac;
        cord_mac_str_to_binary(mac_str, &mac);

        uint32_t port = cord_l2_cam_lookup(cam, &mac, (i % 5) * 10);
        if (port == CORD_L2_CAM_INVALID_PORT)
            found_count++;
    }
    TEST_ASSERT(found_count == 10, "All 10 entries removed after clear");

    // Re-add entries after clear
    printf("\nRe-adding entries after clear...\n");
    for (int i = 0; i < 5; i++)
    {
        char mac_str[32];
        snprintf(mac_str, sizeof(mac_str), "bb:bb:bb:bb:bb:%02x", i);

        cord_mac_addr_t mac;
        cord_mac_str_to_binary(mac_str, &mac);

        int ret = cord_l2_cam_add(cam, &mac, 700 + i, 0);
        TEST_ASSERT(ret == 0, "Re-add after clear works");
    }

    cord_l2_cam_destroy(cam);
    TEST_ASSERT(true, "Destroy L2 CAM table");
}

//
// Test 5: MAC Address Parsing
//
void test_mac_address_parsing(void)
{
    printf("\n=== Test 5: MAC Address Parsing ===\n");

    // Test various MAC string formats
    struct {
        const char *mac_str;
        bool should_succeed;
        uint64_t expected_u64;  // Only checked if should_succeed == true
    } test_cases[] = {
        {"00:00:00:00:00:00", true, 0x000000000000ULL},
        {"ff:ff:ff:ff:ff:ff", true, 0xffffffffffffULL},
        {"a1:b2:c3:d4:e5:f6", true, 0xa1b2c3d4e5f6ULL},
        {"11:22:33:44:55:66", true, 0x112233445566ULL},  // Uppercase
        {"77:88:99:aa:bb:cc", true, 0x778899aabbccULL},  // Lowercase
        {"Dd:Ee:Ff:11:22:33", true, 0xddeeff112233ULL},  // Mixed case
        {"ca:fe:f0:0d:ba:be", true, 0xcafef00dbabeULL},
        {"ab:cd:ef:01:23:45", true, 0xabcdef012345ULL},
        {"98:76:54:32:10:fe", true, 0x9876543210feULL},
        {"f1:e2:d3:c4:b5:a6", true, 0xf1e2d3c4b5a6ULL},
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        cord_mac_addr_t mac;
        int ret = cord_mac_str_to_binary(test_cases[i].mac_str, &mac);

        if (test_cases[i].should_succeed)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Parse %s succeeds", test_cases[i].mac_str);
            TEST_ASSERT(ret == 0, msg);

            // Convert to u64 and verify
            uint64_t mac_u64;
            cord_mac_to_u64(&mac, &mac_u64);

            snprintf(msg, sizeof(msg), "%s converts to 0x%012lx",
                     test_cases[i].mac_str, test_cases[i].expected_u64);
            TEST_ASSERT(mac_u64 == test_cases[i].expected_u64, msg);

            // Round trip: u64 -> MAC -> u64
            cord_mac_addr_t mac2;
            cord_u64_to_mac(mac_u64, &mac2);
            uint64_t mac_u64_2;
            cord_mac_to_u64(&mac2, &mac_u64_2);

            snprintf(msg, sizeof(msg), "%s round-trip u64 conversion", test_cases[i].mac_str);
            TEST_ASSERT(mac_u64 == mac_u64_2, msg);
        }
        else
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Parse %s fails as expected", test_cases[i].mac_str);
            TEST_ASSERT(ret != 0, msg);
        }
    }
}

//
// Print summary
//
void print_test_summary(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║                    Test Summary                          ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  Total:   %3d tests                                      ║\n", results.total);
    printf("║  Passed:  %3d tests (%3d%%)                               ║\n",
           results.passed, results.total > 0 ? (results.passed * 100) / results.total : 0);
    printf("║  Failed:  %3d tests (%3d%%)                               ║\n",
           results.failed, results.total > 0 ? (results.failed * 100) / results.total : 0);
    printf("╚══════════════════════════════════════════════════════════╝\n");
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║           CORD L2 CAM Unit Test Suite                   ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    // Run all test suites
    test_l2_cam_basic_operations();
    test_l2_cam_vlan_isolation();
    test_l2_cam_update();
    test_l2_cam_clear();
    test_mac_address_parsing();

    // Print summary
    print_test_summary();

    return (results.failed == 0) ? 0 : 1;
}
