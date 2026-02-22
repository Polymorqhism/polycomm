#include <stdio.h>
#include <sodium.h>
#include <string.h>
#include "polycomm.h"
#include "client.h"
#include "server.h"
#include "util.h"
#include <signal.h>
#include <curl/curl.h>

#define VERSION_URL "https://raw.githubusercontent.com/Polymorqhism/polycomm/main/version.txt"
struct write_buf {
    char data[32];
    size_t len;
};

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct write_buf *buf = userdata;
    size_t total = size * nmemb;
    if(buf->len + total >= sizeof(buf->data)) return 0;
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    return total;
}
int check_updates()
{
    int needs_update = 0;
    CURL *curl;
    curl = curl_easy_init();
    if(!curl) {
        return 2;
    }
    struct write_buf buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, VERSION_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    CURLcode result;
    result = curl_easy_perform(curl);

    if(result != CURLE_OK) {
        curl_easy_cleanup(curl);
        return 2;
    }


    char local_version[32] = {0};
    FILE *f = fopen("version.txt", "r");
    if(!f) return 2;
    fgets(local_version, sizeof(local_version), f);
    fclose(f);
    local_version[strcspn(local_version, "\r\n")] = '\0';
    buf.data[strcspn(buf.data, "\r\n")] = '\0';
    if(strcmp(local_version, buf.data) != 0) {
        needs_update = 1;
    }

    printf("Current: '%s'\n", local_version);
    printf("Best: '%s'\n", buf.data);
    curl_easy_cleanup(curl);
    return needs_update;
}

int main(void)
{

    int needs_update = check_updates();
    if(needs_update == 1) { // better to be explicit than rely on (x) == 1 being always true :broken_heart:
        puts("WARNING: Your version of polycomm is not correct. To ensure that your use of this program is safe, please consider updating.\n\nIf you wish to continue despite these risks, click ENTER.");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    } else if(needs_update == 2) {
        puts("cURL failed to pull the current version. You are at risk of having an older version which may affect the safety of your computer.\n\nIf you understand the risk, press ENTER.");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }

    if(sodium_init() == -1) {
        puts("Failed to init sodium, cannot continue.");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    display_banner();

    int choice = get_choice();
    if(choice < 0) {
        puts("Invalid.");
        return 1;
    }

    if(choice == 1) {
        handle_client_choice();
    } else {
        handle_server_choice();
    }

    return 0;
}
