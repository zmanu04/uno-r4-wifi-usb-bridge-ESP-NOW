#include "at_handler.h"
#include "cmds_esp_now.h"
#include <esp_now.h>
#include <WiFi.h>

static CAtHandler* g_atHandler = nullptr;

static bool parse_mac(const std::string& mac_str, uint8_t* mac) {
    if (mac_str.length() != 17) return false;
    int values[6];
    if (sscanf(mac_str.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)values[i];
    return true;
}

static void esp_now_on_receive(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (!g_atHandler) return;
    std::vector<uint8_t> dataCopy(incomingData, incomingData + len);
    std::vector<uint8_t> macCopy(mac, mac + 6);

    // Pass the work to the main loop to safely use the Serial/AT port
    g_atHandler->addTask([macCopy, dataCopy]() {
        g_atHandler->send_espnow_recv(macCopy, dataCopy);
    });
}

void CAtHandler::send_espnow_recv(const std::vector<uint8_t>& mac, const std::vector<uint8_t>& data) {
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "+ESPNOWRECV:%02X:%02X:%02X:%02X:%02X:%02X,%d,",
       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], data.size());
    at_srv.write_cstr(prefix);
    at_srv.write_vec8(data);
    at_srv.write_line_end();
}

void CAtHandler::add_cmds_esp_now() {
    g_atHandler = this;

    command_table["+ESPNOWINIT"] = [this](chAT::Server& srv, chAT::ATParser& parser) {
        if (parser.cmd_mode == chAT::CommandMode::Run) {
            WiFi.mode(WIFI_STA);
            if (esp_now_init() != ESP_OK) {
                return chAT::CommandStatus::ERROR;
            }
            esp_now_register_recv_cb(esp_now_on_receive);
            return chAT::CommandStatus::OK;
        }
        return chAT::CommandStatus::ERROR;
    };

    command_table["+ESPNOWDEINIT"] = [this](chAT::Server& srv, chAT::ATParser& parser) {
        if (parser.cmd_mode == chAT::CommandMode::Run) {
            esp_now_unregister_recv_cb();
            if (esp_now_deinit() != ESP_OK) {
                return chAT::CommandStatus::ERROR;
            }
            return chAT::CommandStatus::OK;
        }
        return chAT::CommandStatus::ERROR;
    };

    command_table["+ESPNOWADDPEER"] = [this](chAT::Server& srv, chAT::ATParser& parser) {
        if (parser.cmd_mode == chAT::CommandMode::Write && parser.args.size() >= 2) {
            esp_now_peer_info_t peerInfo = {};
            if (!parse_mac(parser.args[0], peerInfo.peer_addr)) {
                return chAT::CommandStatus::ERROR;
            }
            peerInfo.channel = std::stoi(parser.args[1]);
            peerInfo.encrypt = false;
            
            if (esp_now_add_peer(&peerInfo) != ESP_OK) {
                return chAT::CommandStatus::ERROR;
            }
            return chAT::CommandStatus::OK;
        }
        return chAT::CommandStatus::ERROR;
    };

    command_table["+ESPNOWDELPEER"] = [this](chAT::Server& srv, chAT::ATParser& parser) {
        if (parser.cmd_mode == chAT::CommandMode::Write && parser.args.size() == 1) {
            uint8_t mac[6];
            if (!parse_mac(parser.args[0], mac)) {
                return chAT::CommandStatus::ERROR;
            }
            if (esp_now_del_peer(mac) != ESP_OK) {
                return chAT::CommandStatus::ERROR;
            }
            return chAT::CommandStatus::OK;
        }
        return chAT::CommandStatus::ERROR;
    };

    command_table["+ESPNOWSEND"] = [this](chAT::Server& srv, chAT::ATParser& parser) {
        if (parser.cmd_mode == chAT::CommandMode::Write && parser.args.size() == 3) {
            uint8_t mac[6];
            if (!parse_mac(parser.args[0], mac)) {
                return chAT::CommandStatus::ERROR;
            }
            int data_len = std::stoi(parser.args[1]);
            const std::string& data = parser.args[2];
            
            if (data.length() != data_len) {
                // If lengths don't match, the data might contain parsed delimiters
                return chAT::CommandStatus::ERROR;
            }
            
            if (esp_now_send(mac, (const uint8_t*)data.data(), data_len) != ESP_OK) {
                return chAT::CommandStatus::ERROR;
            }
            return chAT::CommandStatus::OK;
        }
        return chAT::CommandStatus::ERROR;
    };
}