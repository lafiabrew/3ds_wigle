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

    /* Verify short UI labels, account JSON round-trip, and session reset */
    assert(strcmp(wg_auth_short_label(WG_AUTH_WPA2_AES), "WPA2") == 0);
    assert(strcmp(wg_auth_short_label(WG_AUTH_3DS_UDS), "3DS-UDS") == 0);
    char ts[24];
    wg_format_current_utc(ts);
    assert(strlen(ts) == 19);

    snprintf(st.account.username, sizeof(st.account.username), "SCHWARTZKAMEL");
    snprintf(st.account.api_name, sizeof(st.account.api_name), "AID_TEST_12345");
    st.account.rank = 420u;
    st.account.month_rank = 19u;
    st.account.discovered_wifi = 12345ULL;
    st.account.total_uploads = 17u;
    snprintf(st.account.last_sync_utc, sizeof(st.account.last_sync_utc), "2026-11-02 09:15:00");
    assert(wg_save_account(&st));

    memset(&st.account, 0, sizeof(st.account));
    assert(wg_load_account(&st));
    assert(strcmp(st.account.username, "SCHWARTZKAMEL") == 0);
    assert(st.account.rank == 420u);
    assert(st.account.discovered_wifi == 12345ULL);

    assert(wg_reset_session(&st));
    assert(st.ap_count == 0 && st.csv_rows_written == 0);

    /* Fail-the-Fail negative assertions */
    assert(wg_init(NULL, "build/test_sd", "bad.csv") == false);
    assert(wg_record_observation(&st, NULL, "SSID", WG_AUTH_OPEN, 6, -50, ts, false) == false);
    assert(wg_record_observation(&st, "", "SSID", WG_AUTH_OPEN, 6, -50, ts, false) == false);
    assert(wg_reset_session(NULL) == false);
    assert(wg_load_account(NULL) == false);

    printf("PASS: test_wigle all WiGLE-Wifi-1.4 CSV format, account sync JSON, and pass/fail checks succeeded.\n");
    return 0;
}
