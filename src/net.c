#include <osapi.h>
#include <user_interface.h>
#include <espconn.h>

#include <printer.h>

#define str(...) #__VA_ARGS__

const char *INDEX_SOURCE = str(
    <canvas id='img' width='160' height='144'></canvas>
    <br>
    <button onclick='read_img()'>READ</button>
    <style>
        body {
            text-align: center;
        }

        canvas {
            background-color: #3f3f3f;

            width: 75vmin;
            margin-bottom: 2vh;

            image-rendering: pixelated;
        }

        button {
            width: 75vmin;
            height: 5vw;

            font-size: 2vw;
        }
    </style>
    <script defer>
        const cavnas = document.querySelector('#img');
        const ctx = cavnas.getContext('2d');

        function set_pixel(img, x, y, color) {
            const pos = x * 4 + y * 4 * 160;

            img.data[pos]     = color[0];
            img.data[pos + 1] = color[1];
            img.data[pos + 2] = color[2];
            img.data[pos + 3] = 255;
        }

        async function read_img() {
            let data = new Uint8Array(await (await (await fetch('/img')).blob()).arrayBuffer());

            let inp = [];

            for (let i = 0; i < data.length; i += 2)
                for (let bit = 7; bit >= 0; bit--)
                    inp.push(
                        ((data[i] & (1 << bit)) >> bit) + (((data[i + 1] & (1 << bit)) >> bit) << 1)
                    );

            inp = inp.map((x) => { return [[255, 255, 255], [191, 191, 191], [127, 127, 127], [63, 63, 63]][x]; });

            let out = ctx.createImageData(160, 144);

            for (let y = 0; y < 18; y++)
                for (let x = 0; x < 20; x++)
                    for (let dy = 0; dy < 8; dy++)
                        for (let dx = 0; dx < 8; dx++)
                            set_pixel(
                                out, 
                                x * 8 + dx, 
                                y * 8 + dy, 
                                inp[(x * 8 * 8 + y * 8 * 8 * 20) + dx + dy * 8]
                            );

            ctx.putImageData(out, 0, 0);   
        }
    </script>
);

esp_tcp tcp = {
    .local_port = 80,
};

struct espconn web = {
    .type = ESPCONN_TCP,
    .state = ESPCONN_NONE,
    .proto.tcp = &tcp,
};

extern printer_t printer;

void ICACHE_FLASH_ATTR web_recv(void *arg, char *data, uint16_t len);
void ICACHE_FLASH_ATTR web_connect(void *arg);

void ICACHE_FLASH_ATTR init_net(void) {
    wifi_softap_dhcps_stop();

    wifi_set_opmode(SOFTAP_MODE);

    struct ip_info ip_info;

    IP4_ADDR(&ip_info.ip, 69, 69, 69, 69);
    IP4_ADDR(&ip_info.gw, 69, 69, 69, 69);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    wifi_set_ip_info(SOFTAP_IF, &ip_info);

    struct softap_config conf = {
        .ssid = "Gameboy Camera",
        .ssid_len = strlen("Gameboy Camera"),
        .password = { 0 },
        .authmode = AUTH_OPEN,
        .max_connection = 8,
        .beacon_interval = 100,
    };

    wifi_softap_set_config(&conf);

    wifi_softap_dhcps_start();

    espconn_regist_connectcb(&web, web_connect);

    os_printf("Web Start: %d\n", espconn_accept(&web));
}

void ICACHE_FLASH_ATTR web_connect(void *arg) {
    os_printf("HTTP connection!\n");

    struct espconn *client = arg;
    espconn_set_opt(client, ESPCONN_NODELAY);

    espconn_regist_recvcb(client, web_recv);
}

void ICACHE_FLASH_ATTR web_recv(void *arg, char *data, uint16_t len) {
    struct espconn *client = arg;

    if (memcmp("GET", data, 3) != 0) return;

    data += 4;

    char buffer[2000];

    if (!memcmp("/ ", data, 2)) {
        os_sprintf(
            buffer, 
            "HTTP/1.0 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n%s", 
            strlen(INDEX_SOURCE),
            INDEX_SOURCE
        );

        espconn_send(client, buffer, strlen(buffer));
    } else if (!memcmp("/img ", data, 5)) {
        os_sprintf(
            buffer, 
            "HTTP/1.0 200 OK\nContent-Type: application/octet-stream\nContent-Length: %d\n\n", 
            printer.image_pos
        );

        memcpy(printer.image - strlen(buffer), buffer, strlen(buffer));
        espconn_send(client, printer.image - strlen(buffer), strlen(buffer) + printer.image_pos);
    }
}
