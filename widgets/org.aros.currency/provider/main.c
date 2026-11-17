/* SPDX-License-Identifier: MIT */
/* CurrencyProvider: answers one request of the widget protocol.
 *
 *   CurrencyProvider "<request file>"
 *
 * Operation "rates": fetches the European Central Bank's daily reference
 * rates and answers with
 *   - one row per currency (label: its code, value: how much of it one euro
 *     buys), for a host that converts by itself, and
 *   - the fields "date" and "base", and for the pair named in the settings
 *     ("from", "to", default EUR and USD) "pair" ("EUR/USD"), "rate" and
 *     "summary" ("1 EUR = 1.1014 USD, ECB 2026-11-16"), for a host that
 *     shows one number. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rates.h"
#include "zk_https.h"
#include "zk_json.h"
#include "zk_widget_snapshot.h"

#ifdef __AROS__
#include <dos/dos.h>
#include <proto/dos.h>
#endif

#define ECB_URL "https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml"

static int str(const struct ZkJson *j, int o, const char *k, char *v, size_t cap)
{
    return zk_json_string(j, zk_json_get(j, o, k), v, cap);
}

/* The bundled certificates are <package>/certs/ca-bundle.crt; the host
 * names the package directory in the request. */
static void find_package_ca(const struct ZkJson *j)
{
    char root[512], path[512];
    FILE *f;
    int byte;
    zk_https_package_ca[0] = 0;
    if (!str(j, 0, "packageRoot", root, sizeof root) || !*root) return;
#ifdef __AROS__
    snprintf(path, sizeof path, "%s", root);
    if (!AddPart((STRPTR)path, (CONST_STRPTR)"certs/ca-bundle.crt", sizeof path)) return;
#else
    if (snprintf(path, sizeof path, "%s/certs/ca-bundle.crt", root) >= (int)sizeof path) return;
#endif
    f = fopen(path, "rb");
    byte = f ? fgetc(f) : EOF;
    if (f) fclose(f);
    if (byte != EOF) snprintf(zk_https_package_ca, sizeof zk_https_package_ca, "%s", path);
}

static double rate_of(const struct currency_rates *r, const char *code)
{
    int i;
    if (!strcmp(code, "EUR")) return 1;
    for (i = 0; i < r->n; i++)
        if (!strcmp(r->code[i], code)) return r->rate[i];
    return 0;
}

/* A currency code from the settings: three letters, upper case; the
 * default when the setting is missing or is not one. */
static void code_setting(const struct ZkJson *j, int settings, const char *key, const char *fallback, char out[4])
{
    char text[16];
    int i, ok = settings >= 0 && zk_json_string(j, zk_json_get(j, settings, key), text, sizeof text) && strlen(text) == 3;
    for (i = 0; ok && i < 3; i++) {
        if (text[i] >= 'a' && text[i] <= 'z') text[i] = (char)(text[i] - 32);
        if (text[i] < 'A' || text[i] > 'Z') ok = 0;
    }
    memcpy(out, ok ? text : fallback, 3);
    out[3] = 0;
}

static void pair_fields(const struct ZkJson *j, const char *input, const struct currency_rates *rates, struct ZkWidgetSnapshot *reply)
{
    char from[4], to[4];
    int settings = zk_json_get(j, 0, "settings"), n = reply->field_count;
    double a, b;
    (void)input;
    code_setting(j, settings, "from", "EUR", from);
    code_setting(j, settings, "to", "USD", to);
    a = rate_of(rates, from);
    b = rate_of(rates, to);
    strcpy(reply->fields[n].key, "pair");
    snprintf(reply->fields[n].text, sizeof reply->fields[n].text, "%s/%s", from, to);
    n++;
    strcpy(reply->fields[n].key, "rate");
    strcpy(reply->fields[n + 1].key, "summary");
    if (a > 0 && b > 0) {
        /* as text, four significant digits: short enough for a dock tile */
        snprintf(reply->fields[n].text, sizeof reply->fields[n].text, "%.4g", b / a);
        snprintf(reply->fields[n + 1].text, sizeof reply->fields[n + 1].text, "1 %s = %.4f %s, ECB %s", from, b / a, to, rates->date);
    } else {
        strcpy(reply->fields[n].text, "--");
        snprintf(reply->fields[n + 1].text, sizeof reply->fields[n + 1].text, "The bank lists no rate for %s", a > 0 ? to : from);
    }
    reply->field_count = n + 2;
}

int main(int argc, char **argv)
{
    size_t length;
    char *input, *body = NULL;
    struct ZkJson *j = malloc(sizeof *j);
    struct ZkWidgetSnapshot *reply = calloc(1, sizeof *reply);
    struct currency_rates *rates = malloc(sizeof *rates);
    char operation[32], temporary[ZW_PATH], completed[ZW_PATH];
    const char *why = NULL;
    size_t size = 0;
    int result = 5, g = 0, api = 0, i, a, b, ok;
    FILE *f;

    if (argc != 2) return 5;
    input = zk_widget_read(argv[1], 65536, &length);
    if (!input) why = "cannot read the request file";
    else if (!j || !reply || !rates) why = "out of memory";
    else if (!zk_json_parse(j, input, length)) why = "request is not valid JSON";
    else if (!zk_json_int(j, zk_json_get(j, 0, "api"), &api) || api != 1) why = "unsupported api";
    else if (!str(j, 0, "package", reply->package, sizeof reply->package) || strcmp(reply->package, "org.aros.currency")) why = "wrong package";
    else if (!str(j, 0, "instance", reply->instance, sizeof reply->instance) || !zk_widget_id_valid(reply->instance)) why = "bad instance";
    else if (!str(j, 0, "session", reply->session, sizeof reply->session) || !zk_widget_id_valid(reply->session)) why = "bad session";
    else if (!zk_json_int(j, zk_json_get(j, 0, "generation"), &g) || g < 1) why = "bad generation";
    if (why) { fprintf(stderr, "CurrencyProvider: %s\n", why); goto done; }

    find_package_ca(j);
    snprintf(zk_https_user_agent, sizeof zk_https_user_agent, "AROS-Currency/0.1.0");
    reply->generation = (unsigned long)g;
    reply->status = ZW_ERROR;
    reply->updated = reply->source_time = (double)time(NULL);
    strcpy(reply->title, "Exchange rates");
    if (!str(j, 0, "operation", operation, sizeof operation)) {
        strcpy(reply->message, "Missing operation");
    } else if (strcmp(operation, "rates")) {
        strcpy(reply->message, "Unsupported operation");
    } else if (zk_https_get(ECB_URL, 64u * 1024u, &body, &size, reply->message, sizeof reply->message)) {
        if (currency_parse_ecb(body, size, rates)) {
            reply->status = ZW_READY;
            strcpy(reply->attribution, "European Central Bank");
            strcpy(reply->url, "https://www.ecb.europa.eu/stats/policy_and_exchange_rates/euro_reference_exchange_rates/");
            strcpy(reply->fields[0].key, "date");
            snprintf(reply->fields[0].text, sizeof reply->fields[0].text, "%s", rates->date);
            strcpy(reply->fields[1].key, "base");
            strcpy(reply->fields[1].text, "EUR");
            reply->field_count = 2;
            pair_fields(j, input, rates, reply);
            for (i = 0; i < rates->n && i < ZW_ROWS; i++) {
                snprintf(reply->rows[i].label, sizeof reply->rows[i].label, "%s", rates->code[i]);
                snprintf(reply->rows[i].value, sizeof reply->rows[i].value, "%.6g", rates->rate[i]);
            }
            reply->row_count = i;
        } else {
            strcpy(reply->message, "The bank's file has no rates in the expected form");
        }
    }
    free(body);

    a = snprintf(temporary, sizeof temporary, "%s.tmp", argv[1]);
    b = snprintf(completed, sizeof completed, "%s.done", argv[1]);
    if (a < 0 || b < 0 || (size_t)a >= sizeof temporary || (size_t)b >= sizeof completed) {
        fprintf(stderr, "CurrencyProvider: request path too long\n");
        goto done;
    }
    /* the host gives each request a destination that does not exist yet;
       an answer never replaces a file */
    if ((f = fopen(completed, "rb"))) { fclose(f); goto done; }
    if (!(f = fopen(temporary, "wb"))) { fprintf(stderr, "CurrencyProvider: cannot write the reply\n"); goto done; }
    ok = zk_widget_snapshot_write(f, reply);
    if (fclose(f)) ok = 0;
    if (ok && !rename(temporary, completed)) result = 0;
    else { fprintf(stderr, "CurrencyProvider: cannot publish the reply\n"); remove(temporary); }
done:
    free(input); free(j); free(reply); free(rates);
    return result;
}
