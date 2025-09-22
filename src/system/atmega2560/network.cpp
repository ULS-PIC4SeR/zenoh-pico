#include <Arduino.h>
#include <Ethernet.h>

extern "C" {

#include "zenoh-pico/system/platform.h"
#include "zenoh-pico/transport/transport.h"
#include "zenoh-pico/utils/pointers.h"
#include "zenoh-pico/utils/result.h"

/**
 * @brief Converts an IPv4 address string (e.g., "192.168.1.100") to a uint8_t array.
 *
 * @param ip_str A pointer to the null-terminated string containing the IPv4 address.
 * @param ip_array A pointer to a uint8_t array of size 4 where the converted IP will be stored.
 * @return true if the conversion was successful and the IP address was valid.
 * @return false if the input string was NULL, ip_array was NULL, or the IP address format was invalid.
 */
bool convert_ip_string_to_uint8_array(const char *ip_str, uint8_t *ip_array) {
    if (ip_str == NULL || ip_array == NULL) {
        return false;
    }

    // A valid IPv4 string "XXX.XXX.XXX.XXX" has a minimum length (e.g., "0.0.0.0" is 7 chars)
    // and a maximum length (e.g., "255.255.255.255" is 15 chars).
    // Plus the null terminator.
    size_t len = strlen(ip_str);
    if (len < 7 || len > 15) {
        return false;
    }

    unsigned int octet[4]; // Use unsigned int to read, then cast to uint8_t

    // Use sscanf to parse the string into four unsigned integers
    // The " %u.%u.%u.%u" format string expects four decimal numbers separated by dots.
    // The space before %u is important to consume any leading whitespace if present (though typically not for IP).
    // The %n specifier stores the number of characters successfully parsed so far.
    // This allows us to check if the entire string was consumed, ensuring no extra characters are present.
    int chars_parsed;
    int result = sscanf(ip_str, "%u.%u.%u.%u%n", &octet[0], &octet[1], &octet[2], &octet[3], &chars_parsed);

    if (result != 4) { return false; }

    if (chars_parsed != len) { return false; }

    for (int i = 0; i < 4; i++) {
        if (octet[i] > 255) {
            return false;
        }
        ip_array[i] = (uint8_t)octet[i];
    }

    return true;
}

z_result_t _z_socket_set_non_blocking(const _z_sys_net_socket_t *sock) {
    // There should not be anything blocking in the EthernetClient API
    return _Z_RES_OK;
}

void _z_socket_close(_z_sys_net_socket_t *sock) {sock->_client->stop();}

/*------------------ TCP sockets ------------------*/
z_result_t _z_create_endpoint_tcp(_z_sys_net_endpoint_t *ep, const char *s_address, const char *s_port) {
    z_result_t ret = _Z_RES_OK;

    // Allocate memory for IP address
    ep->_ip = (uint8_t*)malloc(4 * sizeof(uint8_t));
    if (ep->_ip == NULL) {
        return _Z_ERR_GENERIC;
    }

    // Parse, check and add IP address
    if (!convert_ip_string_to_uint8_array(s_address, ep->_ip)) {
        free(ep->_ip);
        ep->_ip = NULL;
        ret = _Z_ERR_GENERIC;
        return ret;
    }

    // Parse, check and add the port
    uint32_t port = strtoul(s_port, NULL, 10);
    if ((port > (uint32_t)0) && (port <= (uint32_t)65355)) {  // Port numbers should range from 1 to 65355
        ep->_port = (uint16_t)port;
    } else {
        ret = _Z_ERR_GENERIC;
    }

    return ret;
}

void _z_free_endpoint_tcp(_z_sys_net_endpoint_t *ep) {
    if (ep->_ip != NULL) {
        free(ep->_ip);
        ep->_ip = NULL;
    }
    ep->_port = -1;
}

z_result_t _z_open_tcp(_z_sys_net_socket_t *sock, const _z_sys_net_endpoint_t rep, uint32_t tout) {
    z_result_t ret = _Z_RES_OK;

    sock->_client = new EthernetClient();
    sock->_client->setConnectionTimeout(tout);

    if (!sock->_client->connect(rep._ip, rep._port)) {
        return _Z_ERR_GENERIC;
    }
    
    return ret;
}

z_result_t _z_listen_tcp(_z_sys_net_socket_t *sock, const _z_sys_net_endpoint_t rep) {
    z_result_t ret = _Z_RES_OK;

    sock->_client = new EthernetClient();

    if (!sock->_client->connect(rep._ip, rep._port)) {
        ret = _Z_ERR_GENERIC;
    }

    // TODO(giafranchini): listen is not implemented for EthernetClient
    // ret = listen(sock->_number);
    // if (ret != SOCK_OK) {
    //     ret = _Z_ERR_GENERIC;
    // }

    return ret;
}

void _z_close_tcp(_z_sys_net_socket_t *sock) {sock->_client->stop();}

size_t _z_read_tcp(const _z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {

    // If socket is disconnected and no bytes pending -> error
    if (!sock._client->connected() && sock._client->available() == 0) {
        return SIZE_MAX;
    }

    int32_t avail = sock._client->available(); // bytes ready to read
    if (avail <= 0) {
        // Nothing to read now. Return 0, not SIZE_MAX.
        return 0;
    }

    if (avail > (int32_t)len) avail = (int32_t)len;
    int32_t n = sock._client->read(ptr, (size_t)avail);
    if (n < 0) {
        // Read error reported by library
        return SIZE_MAX;
    }

    return (size_t)n;
}

size_t _z_send_tcp(const _z_sys_net_socket_t sock, const uint8_t *ptr, size_t len) {
    if (!sock._client || !sock._client->connected()) {
        return -1;
    }

    size_t total = 0;
    unsigned long start = millis();
    while (total < len) {
        int sent = sock._client->write(ptr + total, len - total);
        if (sent < 0) return -1;
        total += (size_t)sent;

        // avoid infinite loop: if nothing gets sent for > 500ms, break
        if ((millis() - start) > 500) {
            break;
        }

        // yield briefly to let SPI/network progress
        delay(1);
    }
    return total;
}

size_t _z_read_exact_tcp(const _z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    // Copied from FreeRTOS+TCP
    // Serial.println("Read exact TCP");
    unsigned long start = millis();
    size_t n = 0;
    uint8_t *pos = &ptr[0];

    do {
        size_t rb = _z_read_tcp(sock, pos, len - n);
        if (rb == 0) {
            // nothing yet, give W5100 time
            if (millis() - start > 1000) { // 1s timeout
                return SIZE_MAX; // timeout
            }
            // Yield/short delay instead of Serial.print
            delayMicroseconds(200);
            continue;
        } else if (rb == SIZE_MAX) {
            n = rb;
            break;
        }

        n = n + rb;
        pos = _z_ptr_u8_offset(pos, n);
    } while (n != len);

    return n;
}

}  // extern "C"
