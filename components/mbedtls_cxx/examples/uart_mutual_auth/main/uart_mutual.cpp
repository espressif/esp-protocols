/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <thread>
#include <sys/socket.h>
#include <unistd.h>
#include <esp_check.h>
#include "esp_log.h"
#include "mbedtls_wrap.hpp"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_netif.h"

static auto const *TAG = "uart_mutual_tls";

const unsigned char cacert[] = "-----BEGIN CERTIFICATE-----\n"
                               "MIIDIzCCAgugAwIBAgIURgnkf/YFHeJCEyGZNm1I1hd34xcwDQYJKoZIhvcNAQEL\n"
                               "BQAwITELMAkGA1UEBhMCQ1oxEjAQBgNVBAMMCUVzcHJlc3NpZjAeFw0yNDAyMTUx\n"
                               "MTMyMjVaFw0yNTAyMTQxMTMyMjVaMCExCzAJBgNVBAYTAkNaMRIwEAYDVQQDDAlF\n"
                               "c3ByZXNzaWYwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDI3UdvPpmi\n"
                               "56OIK0AOt/RorRXLM7hfyWfkuQjT4fdqb6eUVBQ3PMt3h2cipTlAWYY11gigdikV\n"
                               "59eO5SEJ3ehGZlExgIlGklHme4G7fmL6uLjuzEmLavz8aanKbTqGFxfjbCkTf6tD\n"
                               "OJkT+lpBGrYuC8/OMjoxgHhxQYxPBKJENH6VxjMwrrH3rO2BMQOJolEZpYpyv5nE\n"
                               "avn8iFJk3Gew9P8mkGTvdJ9UCyq5P9aPowbNvNlT+46OTeB/PyrLbIEcSw5rmjrh\n"
                               "5cOCunu0pZmKEw3gJU7Kb5T+DyegJCQPGy+MVkTQ/qoAgJMfY3CZCLPbWUGx/uxt\n"
                               "MLMq6ysSVHwZAgMBAAGjUzBRMB0GA1UdDgQWBBT54IyCKBrnW+0MNgH/z3wFJWeO\n"
                               "mjAfBgNVHSMEGDAWgBT54IyCKBrnW+0MNgH/z3wFJWeOmjAPBgNVHRMBAf8EBTAD\n"
                               "AQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBg3WlfNiKoolSmJe01zhos66HduqX4kR3s\n"
                               "W8/aD/hd4QLOLrierSXuVn8yllIUjs6mMK4iZNH2PYKCfwEvWm+gIsrzrHWfyhtY\n"
                               "wAbQUJ3EweqHyHqh7SYly5fvgsbbKlwGIwFu/IEOjnsTyG8uwPLi3vxPYQGrCOJR\n"
                               "QNeKmwIGhZZyZSnFOtyndvhnMa6ykLjv6wIcaaS2muVS9yr/UEd52GLSU1h4iT3q\n"
                               "Sn5Fcmd0uxZ6yuRN5s0WcUTPhmuAMAQkP66mkJkI6RaoxauruDpo0HsSAXC12UKG\n"
                               "X7+F8ekWeRO1Sd1nS0fZG795chnuH4LBiXBzFvKkhZPoDo7fr0lo\n"
                               "-----END CERTIFICATE-----";
const unsigned char clientcert[] = "-----BEGIN CERTIFICATE-----\n"
                                   "MIICuzCCAaMCFBdHd97jdvI7d4rSVaRZQUPiTTn7MA0GCSqGSIb3DQEBCwUAMCEx\n"
                                   "CzAJBgNVBAYTAkNaMRIwEAYDVQQDDAlFc3ByZXNzaWYwHhcNMjQwMjE1MTEzNDAw\n"
                                   "WhcNMjUwMjE0MTEzNDAwWjATMREwDwYDVQQDDAhNeUNsaWVudDCCASIwDQYJKoZI\n"
                                   "hvcNAQEBBQADggEPADCCAQoCggEBAPZHkjuldGnwtz7jBWhcYU0lvBZHQVZVDG5t\n"
                                   "dcSV56WL3IXfwanQtSOVVQiNGRSUHqlSnZjdDS4qlbeESveeXsvMRRl9QYn+v3G9\n"
                                   "hoe8/HZQDm3F4F2eFYVtXaIjyFsxr+POhy/WVVAOmClRuMcCknMGf7WzozlnoUxW\n"
                                   "PCNtxpgBpHUiEUnSMUlqvtf89DUitwK9GfEbKRebM8c04U56uuXQpQzHX4ksP9hv\n"
                                   "PcRyerc7FiUyX1VFSZ1POmdqrwRqDNB66ZVz7YhFkhj7Am3bZ4F1cZ1oddXHz+3l\n"
                                   "KNUSjYf1sHVINq4lbfIGsyh5m3dv8Rv79pvTBMoR2qCScgkCxIsCAwEAATANBgkq\n"
                                   "hkiG9w0BAQsFAAOCAQEAFalmADbLZdVah/x6ff+8OhUm1QSFk9nU+1vvTC59T7ua\n"
                                   "mUIRDB2gAWzCt/pfABLXZtTD33DDDSz3T1rRJV/NhI3RrHZaBK1HWHPTqdIx5zzS\n"
                                   "WZHL9xgZf5RAHkLFI+2lhyXzWDwHe6H38+ZUPTi2S5v5pMQqR4f1HKwDJX3yrOdF\n"
                                   "dQhMgP4wHWT6sug8w+x3nSm3UV3STVQWwWMcXPtyrSSMBnJFH1sNOi2+0M99NEc3\n"
                                   "IaFwScvaMFktJnOeXoBS+bKL2UiUq8OILyv16v0LA1KDouvJ1mVjM3wky7VaTtbA\n"
                                   "UUsjdD8UaoEnBAGjTe/zJsUN2u1qdH5pU5ycYB/mpA==\n"
                                   "-----END CERTIFICATE-----";
const unsigned char clientkey[] = "-----BEGIN RSA PRIVATE KEY-----\n"
                                  "MIIEpQIBAAKCAQEA9keSO6V0afC3PuMFaFxhTSW8FkdBVlUMbm11xJXnpYvchd/B\n"
                                  "qdC1I5VVCI0ZFJQeqVKdmN0NLiqVt4RK955ey8xFGX1Bif6/cb2Gh7z8dlAObcXg\n"
                                  "XZ4VhW1doiPIWzGv486HL9ZVUA6YKVG4xwKScwZ/tbOjOWehTFY8I23GmAGkdSIR\n"
                                  "SdIxSWq+1/z0NSK3Ar0Z8RspF5szxzThTnq65dClDMdfiSw/2G89xHJ6tzsWJTJf\n"
                                  "VUVJnU86Z2qvBGoM0HrplXPtiEWSGPsCbdtngXVxnWh11cfP7eUo1RKNh/WwdUg2\n"
                                  "riVt8gazKHmbd2/xG/v2m9MEyhHaoJJyCQLEiwIDAQABAoIBAEXhoRjTpei5qQVr\n"
                                  "HYmzTNi7MFeR+HQqxdA/tv8FGinbOcOy7hzlX8CtCufWQZuZO+oHyzgo4SiMZNch\n"
                                  "7rO8eGGToLfO1t31LxVzFc1GTsyzgqSbVUK7LJgjpEHxrVRTEPmvDKUCSErjGUIA\n"
                                  "MlIl5LBG084XHuWXBinG/mF/MK7ImgYi9sSa2I9N6JPHoype/tg16vP2a3v3CNUu\n"
                                  "YPH8fezsFSBs8FFd8rW7k2Q7qg6Wa5rYvafpMbBtD9cJXAFYqqJ2IAQNpixDwHj5\n"
                                  "D3J5HTurhgC9NvNtQ49KgfKFRohXZF4PZ9XE1Lh4Jat3qo6P4OA3W7iTu1mruJGh\n"
                                  "p1wXemECgYEA/GZqTqXdutrobwtHxmXl5eGQx9/BF9OrgXaOrJCRSIMyxh2F10bA\n"
                                  "VG+k83ppF43dWBGX1yKksFV0uZrPK+lh4eFkPPOC6o3EQmWfQzkCCAZEXVpItL6C\n"
                                  "Mp4v3weeEyfRPmAbgauiDPSJ5ZXbh7jP/eStJa4qLuM430cESlW76l8CgYEA+crO\n"
                                  "7jQKFl4e3We25oboG/mD8QQsGGQdcfC8XkGb6milcRJcAjKyWPBwvPA815gmz9hn\n"
                                  "Qk9/X0Y4ADg+hP7iVoC6BRfQYdSJ1hsUtNxLv49V1waQCYxkvZJ5ajjX95KOU7i4\n"
                                  "/Pbim6wWrpN66trzN40YzDGQHLJiAmEtxtDc7VUCgYEA7CDkU6/ZQHaL/VcQTwwF\n"
                                  "iIr+Z/9tJl1glj3UPJ0DTlNvrOjxzfTi+ht4tlBPATo3Wa0b4KkIae+IxBuQtgQh\n"
                                  "DrFOlbc7QzRd58AqvzkWLWuviaZtXqrcI37aSk1WFZWqrDA9i5KGiJg+agtI1jCQ\n"
                                  "ZXcKhbXqwPLSwhAuc1zB8QECgYEA5h8I9DnM8T5UgPSDc2zleKAuBWQqm23gEpAN\n"
                                  "eWhIE3PEtp6LVRsPYxBfTDCmXJg3aVOcDWLfnQ47mTg3oJ6QNdDxjq+Zsgbz1OOt\n"
                                  "99Dbl+ac1jOdjq5gQKUoZctoaxQBOu/6vFFWAsRPQRVtL9/2IT9DkRo4Abf0wux0\n"
                                  "F61jWuECgYEAlEFSF00+uGbcFuW7WwH4SdC+cjwGnFxf6Qr52mGiZYiiHkrMJb8o\n"
                                  "j//OXqzZqo2uErlhRC+B2t+q5ZOWJwUrAXv90uAtPBuJGC4RcE568EvKGuFd2g8e\n"
                                  "Udr/NBw+ghrSuI/sSqB7MAzqcwkhX+BGtTy2kSS6V5kfAhjXBidbJoc=\n"
                                  "-----END RSA PRIVATE KEY-----";



const unsigned char servercert[] = "-----BEGIN CERTIFICATE-----\n"
                                   "MIICvDCCAaQCFBdHd97jdvI7d4rSVaRZQUPiTTn6MA0GCSqGSIb3DQEBCwUAMCEx\n"
                                   "CzAJBgNVBAYTAkNaMRIwEAYDVQQDDAlFc3ByZXNzaWYwHhcNMjQwMjE1MTEzMzM0\n"
                                   "WhcNMjUwMjE0MTEzMzM0WjAUMRIwEAYDVQQDDAlteV9zZXJ2ZXIwggEiMA0GCSqG\n"
                                   "SIb3DQEBAQUAA4IBDwAwggEKAoIBAQCqGKRqOsPIwpFC6dZa/UeC8SNbBxs48YVP\n"
                                   "n+elMypx9GHtemTWlrcjdy/K/iaheSPFEJI368g2tS/yrx/OSIWp+6uC2WdC4iaa\n"
                                   "JvSI0ZDf8lPgriPZsE/89aulcfKCSQAdbVx0aTlwYdlCYQXkFGTKy00l2/AEhmPM\n"
                                   "9L639qy1APHxVtiRCGtg9zruBLdDgJ51O6N0yFMo1lmSoT4F6CWYKsG4/8iP75Bj\n"
                                   "LjK0wDBVYSdDcYADkKpPlhenY+ZEEiMthUQjA3vnNSdCWrGIR2Zb+QTwA20gMdDU\n"
                                   "8+eun5LPNCMFfhzD5j478WL7Gfp9wjFFCtBGAcrl8W//RSPznTAtAgMBAAEwDQYJ\n"
                                   "KoZIhvcNAQELBQADggEBAIxVz9aNbjlYzaX91mQ6CLwoAeBmAzl8Ck4L5pSz9X+1\n"
                                   "A2d4BM+diQjlADNldH+w6w7j7JolxUBkJSFGMAMXSvqNcu6ORhWb9MjC+x/aRYTZ\n"
                                   "hf6qEHl80nP5lTMl8Hy0fYMzG75SfhXYhzhz7Z/GcFC4SQWj7Zv7k/bFYR5JbhIg\n"
                                   "WqF1dv4xffbGa4CMAkaWgBvgI/Wq+XW+mFJPrKzYBsKcu9HbeeZozuo9goVkoRba\n"
                                   "uEESPeNWZv/1iSly2kwcvK6TuuI4I4z3yFqZFj5fhUtjGhaPlJC9LcI7jzJa+K5Y\n"
                                   "EDFTvDQzmqJUUAg8aWVI/Wut1ji9QEEzY+SADz6fP5Y=\n"
                                   "-----END CERTIFICATE-----";

const unsigned char serverkey[] = "-----BEGIN RSA PRIVATE KEY-----\n"
                                  "MIIEogIBAAKCAQEAqhikajrDyMKRQunWWv1HgvEjWwcbOPGFT5/npTMqcfRh7Xpk\n"
                                  "1pa3I3cvyv4moXkjxRCSN+vINrUv8q8fzkiFqfurgtlnQuImmib0iNGQ3/JT4K4j\n"
                                  "2bBP/PWrpXHygkkAHW1cdGk5cGHZQmEF5BRkystNJdvwBIZjzPS+t/astQDx8VbY\n"
                                  "kQhrYPc67gS3Q4CedTujdMhTKNZZkqE+BeglmCrBuP/Ij++QYy4ytMAwVWEnQ3GA\n"
                                  "A5CqT5YXp2PmRBIjLYVEIwN75zUnQlqxiEdmW/kE8ANtIDHQ1PPnrp+SzzQjBX4c\n"
                                  "w+Y+O/Fi+xn6fcIxRQrQRgHK5fFv/0Uj850wLQIDAQABAoIBAHXcW1isXWsnvoWy\n"
                                  "B/DGXZ3Svt/dPbSoTepNb7JdkMSjRJPL4kF6721osbojftsWWH29LMQI4ZNe2tl7\n"
                                  "FTvXrp6JH1+sisuiboMUCQ8gvxUeEZa2s2qsq9Ao3oXmPdafBLBfTdfv7Xf8pRFE\n"
                                  "r1NJ+kk2s79O9bH8+PxUfi50g1lrK8LG06jkJpuAbzKg8c9bJJ5LzBzYzw8wO7Tp\n"
                                  "POvUwCT51/NjR5LxqzTgD4ckPuYVyp/avkg2biaOO2xXCCOx6jNotUaKj6OpmB4n\n"
                                  "M95/kVvR1+TwlfS1rSQVUUJXsawUz5iWNroM8YEoxBQ/WA2CGj/w59Oxn446eRbE\n"
                                  "EPrq8zUCgYEA2g8NC+ZOQMskmdPAX+/wu88CqN1JXOOTTannIjT+XRmvFnI2IcR2\n"
                                  "Qsz2hSKsNbhJH4yKqmAgncTXFdAadKt5ZjQcLFd9JeG/GoLcTzt6IGgRzl1LKl5Y\n"
                                  "YMSV5SBI16m9HBXgWnzNgvct67zmtUucFMqSMdMxknzJbG9FYAghL/cCgYEAx7E0\n"
                                  "rHZHJMZpSmBg5dDO9l3QPrRxxtIQy5o5+5YvQ6fpk4N5F+KtDye3oLjKld2k02sV\n"
                                  "DJYfr5B9DLqsrRskQM8F/q9D8Az/PLfUpbmZ6WVN0jTAu//HPbZHa0LpH3to+B/S\n"
                                  "nnkdDWHlZs9TvuQr5YZXuGqnTAvyhkOoEkNN3/sCgYA8DcMZEN9iRtAYsVGc2lbh\n"
                                  "Uly4JuFqfJ532B/4ssGO4GDw/Jld6V5sfUgzWF43GT7COpGB5KF28dwOfNacZRE1\n"
                                  "DYrox1uHEEnyQjHsfEPhIugsflMSIxOR6vIhPSfyhSO41WmJYi+zLuHtt4OOUHl2\n"
                                  "3Gcw46oWXtmWTHq9vN9u9wKBgGiLzt7nwZFwSxmEYdaPvnrfXLIneFW2DtL5eJfN\n"
                                  "5grOswvmzhQCOcZwbcO4W1+gvbVuH4QKaKZayA1NAjBSwGUpvaK8EZ5wv4QDXlIx\n"
                                  "XHID9n0x3yHN5HrbnoJ6cmBoFOmqh3MuR1aFRTvRGbAb9xtgfTZwqAu5SYyfiTOe\n"
                                  "hvvXAoGAfYGO51AR1qfzYQh5nhm4/gFRESUbZevpNDF610GCXNhrcFWH0WgL5rUo\n"
                                  "bUPvhje/iRpmarv4vB75ax9wTpBp+Wl08YDx4sHlBUAi5JYvieyotfxjI359jAE5\n"
                                  "X+RPrWjoMOX3QUxCQt5D1gnMi9msFzpsZaEOkeMKf/DTjNNREFA=\n"
                                  "-----END RSA PRIVATE KEY-----";

/**
 * Using DTLS the below is set to true.
 * In that case, we need to receive the entire datagram, not a fragment
 * This defines a very simple datagram protocol over UART:
 *  | HEADER (2bytes) | PAYLOAD ...   |
 *  | dgram_len       | dgram_payload |
 *
 * If `use_dgrams` is set to false, we perform TLS on UART stream.
 * The UART driver is already a stream-like API (using ringbufer), so we simple read and write to UART
 */
static const bool use_dgrams = false;

class SecureLink: public Tls {
public:
    explicit SecureLink(uart_port_t port, int tx, int rx) : Tls(), uart(port, tx, rx) {}
    ~SecureLink() = default;
    int send(const unsigned char *buf, size_t len) override
    {
        if (use_dgrams) {
            // sends a separate dgram header
            uint16_t header = len;
            uart_write_bytes(uart.port_, &header, 2);
        }
        return uart_write_bytes(uart.port_, buf, len);
    }

    int recv(unsigned char *buf, size_t len) override
    {
        // stream read
        return uart.recv(buf, len, 0);
    }

    int recv_tout(unsigned char *buf, size_t len, int timeout) override
    {
        // dgram read
        return uart.recv_dgram(buf, len, timeout);
    }

    bool open(bool server_not_client)
    {
        if (uart.init() != ESP_OK) {
            return false;
        }
        while (!uart.debounce(server_not_client)) {
            printf("debouncing...\n");
            usleep(10000);
        }
        TlsConfig config{};
        config.is_dtls = use_dgrams;
        config.timeout = 10000;
        if (server_not_client) {
            const unsigned char client_id[] = "Client1";
            config.client_id = std::make_pair(client_id, sizeof(client_id));
        }
        if (!init(is_server{server_not_client}, do_verify{true}, &config)) {
            return false;
        }

        return handshake() == 0;
    }

private:
    /**
     * RAII wrapper of UART
     */
    struct uart_info {
        uart_port_t port_;
        QueueHandle_t queue_{};
        int tx_, rx_;

        // used for datagrams
        bool header_{true};
        int in_payload_{0};
        int payload_len_{0};
        uint8_t payload_[1600] {};

        explicit uart_info(uart_port_t port, int tx, int rx): port_(port), tx_(tx), rx_(rx)
        {
        }
        esp_err_t init()
        {
            uart_config_t uart_config = {};
            uart_config.baud_rate = 115200;
            uart_config.data_bits = UART_DATA_8_BITS;
            uart_config.parity    = UART_PARITY_DISABLE;
            uart_config.stop_bits = UART_STOP_BITS_1;
            uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
            uart_config.source_clk = UART_SCLK_DEFAULT;
            ESP_RETURN_ON_ERROR(uart_driver_install(port_, 1024, 0, 1, &queue_, 0), TAG, "Failed to install UART");
            ESP_RETURN_ON_ERROR(uart_param_config(port_, &uart_config), TAG, "Failed to set params");
            ESP_RETURN_ON_ERROR(uart_set_pin(port_, tx_, rx_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "Failed to set UART pins");
            ESP_RETURN_ON_ERROR(uart_set_rx_timeout(port_, 10), TAG, "Failed to set UART Rx timeout");
            return ESP_OK;
        }
        ~uart_info()
        {
            uart_driver_delete(port_);
        }
        bool debounce(bool server)
        {
            uint8_t data = 0;
            if (server) {
                while (uart_read_bytes(port_, &data, 1, 0) != 0) {
                    if (data == 0x55) {
                        uart_write_bytes(port_, &data, 1);
                        return true;
                    }
                }
                return false;
            }
            data = 0x55;
            uart_write_bytes(port_, &data, 1);
            data = 0;
            uart_read_bytes(port_, &data, 1, pdMS_TO_TICKS(1000));
            if (data != 0x55) {
                uart_flush_input(port_);
                return false;
            }
            return true;
        }

        int recv(unsigned char *buf, size_t size, int timeout) // this is for stream transport
        {
            int len = uart_read_bytes(port_, buf, size, pdMS_TO_TICKS(timeout));
            if (len == 0) {
                return MBEDTLS_ERR_SSL_WANT_READ;
            }
            return len;
        }

        int recv_dgram(unsigned char *buf, size_t size, int timeout) // this is for datagrams
        {
            uart_event_t event = {};
            size_t length;
            uart_get_buffered_data_len(port_, &length);
            if (length == 0) {
                xQueueReceive(queue_, &event, pdMS_TO_TICKS(timeout));
            }
            uart_get_buffered_data_len(port_, &length);
            if (length == 0) {
                return MBEDTLS_ERR_SSL_WANT_READ;
            }
            if (header_) {
                if (length >= 2) {
                    uart_read_bytes(port_, &payload_len_, 2, 0);
                    header_ = false;
                    length -= 2;
                }
            }
            if (!header_ && length > 0) {
                int to_read = payload_len_ - in_payload_;
                int l = uart_read_bytes(port_, &payload_[in_payload_], to_read, 0);
                in_payload_ += l;
                if (payload_len_ == in_payload_) {
                    header_ = true;
                    memcpy(buf, payload_, payload_len_);
                    in_payload_ = 0;
                    return payload_len_;
                }
            }
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
    } uart;
};

static void tls_client()
{
    const unsigned char message[] = "Hello\n";
    unsigned char reply[128];
    SecureLink client(UART_NUM_2, 4, 5);
    const_buf cert{clientcert, sizeof(clientcert)};
    const_buf key{clientkey, sizeof(clientkey)};
    if (!client.set_own_cert(cert, key)) {
        ESP_LOGE(TAG, "Failed to set own cert");
        return;
    }
    const_buf ca{cacert, sizeof(cacert)};
    if (!client.set_ca_cert(ca)) {
        ESP_LOGE(TAG, "Failed to set peer's cert");
        return;
    }
    if (!client.open(false)) {
        ESP_LOGE(TAG, "Failed to CONNECT! %d", errno);
        return;
    }
    ESP_LOGI(TAG, "client opened...");
    if (client.write(message, sizeof(message)) < 0) {
        ESP_LOGE(TAG, "Failed to write!");
        return;
    }

    int len;
    while ((len = client.read(reply, sizeof(reply))) == MBEDTLS_ERR_SSL_WANT_READ) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to read!");
        return;
    }
    ESP_LOGI(TAG, "Successfully received: %.*s", len, reply);
}

static void tls_server()
{
    unsigned char message[128];
    SecureLink server(UART_NUM_1, 25, 26);
    const_buf cert{servercert, sizeof(servercert)};
    const_buf key{serverkey, sizeof(serverkey)};
    if (!server.set_own_cert(cert, key)) {
        ESP_LOGE(TAG, "Failed to set own cert");
        return;
    }
    const_buf ca{cacert, sizeof(cacert)};
    if (!server.set_ca_cert(ca)) {
        ESP_LOGE(TAG, "Failed to set peer's cert");
        return;
    }
    ESP_LOGI(TAG, "openning...");
    if (!server.open(true)) {
        ESP_LOGE(TAG, "Failed to OPEN! %d", errno);
        return;
    }
    int len;
    while ((len = server.read(message, sizeof(message))) == MBEDTLS_ERR_SSL_WANT_READ) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to read! %x", -len);
        return;
    }
    ESP_LOGI(TAG, "Received from client: %.*s", len, message);
    if (server.write(message, len) < 0) {
        ESP_LOGE(TAG, "Failed to write!");
        return;
    }
    ESP_LOGI(TAG, "Written back");
    vTaskDelay(pdMS_TO_TICKS(500));
}


extern "C" void app_main()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    std::thread t2(tls_server);
    std::thread t1(tls_client);
    t1.join();
    t2.join();
}
