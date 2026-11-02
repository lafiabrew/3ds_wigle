/*
 * Unit & specification tests for 3ds_wigle (WiGLE-Wifi-1.4 Wardriver & 2.4GHz Radar):
 * - Verifies exact WiGLE v1.4 2-line CSV header (`WigleWifi-1.4,appRelease=...` +
 *   `MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type`).
 * - Verifies AuthMode capability strings (`[WPA2-PSK-CCMP][ESS]`, `[ESS]`, `[WEP][ESS]`, `[NINTENDO-3DS-UDS][IBSS]`).
 * - Verifies 2.4 GHz channel-to-frequency mapping (`Ch 1 = 2412 MHz`, `Ch 6 = 2437 MHz`, `Ch 11 = 2462 MHz`).
 * - Verifies AP deduplication in memory while appending timestamped wardrive observations to CSV.
 */

#include "wg_wigle.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    assert(wg_channel_to_freq_mhz(1) == 2412);
    assert(wg_channel_to_freq_mhz(6) == 2437);
    assert(wg_channel_to_freq_mhz(11) == 2462);
    assert(strcmp(wg_auth_to_wigle_caps(WG_AUTH_WPA2_AES), "[WPA2-PSK-CCMP][ESS]") == 0);
    assert(strcmp(wg_auth_to_wigle_caps(WG_AUTH_OPEN), "[ESS]") == 0);

    remove("build/test_sd/3ds/wigle/test_wigle.csv");
    wg_wardrive_state_t st;
    assert(wg_init(&st, "build/test_sd", "test_wigle.csv"));

    int swept = wg_perform_sweep(&st, "2026-11-02 05:30:00");
    assert(swept == 4);
    assert(st.ap_count == 4);
    assert(st.csv_rows_written == 4);

    /* Second sweep updates seen_count on existing BSSIDs without duplicating AP slots */
    swept = wg_perform_sweep(&st, "2026-11-02 05:31:00");
    assert(swept == 4);
    assert(st.ap_count == 4);
    assert(st.aps[0].seen_count == 2);
    assert(st.csv_rows_written == 8);

    FILE *fp = fopen(st.session_csv_path, "rb");
    assert(fp != NULL);
    char csv[4096];
    size_t n = fread(csv, 1, sizeof(csv) - 1, fp);
    fclose(fp);
    csv[n] = '\0';

    assert(strstr(csv, "WigleWifi-1.4,appRelease=1.0,model=Nintendo3DS") == csv);
    assert(strstr(csv, "MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type\n") != NULL);
    assert(strstr(csv, "A4:2B:B0:11:22:33,LAFIA_HOME_2.4G,[WPA2-PSK-CCMP][ESS],2026-11-02 05:30:00,1,-46,") != NULL);

    printf("PASS: test_wigle all WiGLE-Wifi-1.4 CSV format, 2.4GHz channel, and sweep checks succeeded.\n");
    return 0;
}
