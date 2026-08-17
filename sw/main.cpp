#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <windows.h>

#include "serialib.h"
#include "organ.hpp"

#define CTRLS_QTY (7)

using std::cout, std::endl;

bool debug_mode = false;

const unsigned int SAMPLE_RATE = 44100;

std::atomic<bool> UART_connected = false;
std::atomic<bool> UART_running = true;

float tones[7] = {261.63, 293.66, 329.63, 349.23, 392.00, 440.0, 493.88};

AscensionStream organ;

void UART_ReaderThread(const char *port, int baud_rate = 115200)
{
    serialib serial;
    if (debug_mode) {
        cout << "Trying to connect to serial..." << std::endl;
    }
    uint8_t res = serial.openDevice(port, baud_rate);

    if (res != 1) {
        if (debug_mode) {
            cout << "Error connecting to UART on port " << port << endl;
        }
        UART_connected = false;
        return;
    }

    UART_connected = true;
    if (debug_mode) {
        cout << "Successfully connected on port " << port << endl;
    }
    uint8_t buffer[20] = {0};
    while (UART_running) {
        bool is_packet_start = true;
        for (int i{}; i < 2; i++)
        {
            int ctrl_start_received = serial.readBytes(buffer, 1, 1500);
            if(!ctrl_start_received || buffer[0] != 0xAE) {
                is_packet_start = false;

                if (debug_mode) {
                    cout << "Received " << ctrl_start_received << " bytes, " << std::hex << buffer[0] << std::dec << ", breaking packet" << endl;
                }
                break;
            } 
        }

        if (!is_packet_start) continue;

        int bytes_received = serial.readBytes(buffer, 2 + CTRLS_QTY * 2, 1500);

        if (debug_mode) {
            cout << "Packet bytes received: " << bytes_received << ", end = " << std::hex << (int)buffer[CTRLS_QTY * 2] << std::dec << endl;
        }
        std::vector<AscensionStream::VoiceData> currentTones{0};
       
        if (bytes_received == 2 + CTRLS_QTY * 2
            && buffer[CTRLS_QTY * 2] == buffer[CTRLS_QTY * 2 + 1]
            && buffer[CTRLS_QTY * 2] == 0xEB) {
            if (debug_mode) {
                cout << "Received tones: " << endl;
            }
            int toneVolume{0};

            for (int i = 0; i < bytes_received - 2; i += 2) {
                toneVolume = (buffer[i] << 8) | (buffer[i + 1] & 0xFF);
                if (debug_mode) cout << toneVolume << " ";

                AscensionStream::VoiceData tone{
                    .frequency {tones[i / 2]},
                    .volume {toneVolume / 1024.0}
                };
                currentTones.push_back(tone);
            }
            if (debug_mode) cout << endl;
            
            organ.setPolyphony(currentTones);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    serial.closeDevice();
    if (debug_mode) cout << "UART closed" << endl;
}

bool hasFlag(int argc, char *argv[], const char *flagName) {
    for (int i{}; i < argc; i++) {
        if (strncmp(argv[i], flagName, strlen(flagName)) == 0) {
            return true;
        }
    }

    return false;
}

int main(int argc, char *argv[]) {
    if (debug_mode = hasFlag(argc, argv, "-debug")) {
        AllocConsole();
        
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "w", stdin);
        
        SetConsoleOutputCP(65001);
        cout << "Opened in debug mode" << endl;
    }

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(600, 300)), "Light Organ");
    window.setFramerateLimit(60);

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "ImGui init error!" << std::endl;
        return -1;
    }
    ImGui::StyleColorsLight();

    char UART_Port[128] = "";
    sf::Clock deltaClock;

    std::unique_ptr<std::thread> UART_Thread = (nullptr);

    organ.play();

    float organVolume = 0.4f;
    organ.setVolumeLevel(0.2);

    while (window.isOpen()) {
        
        while (std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            if ((*event).is<sf::Event::Closed>()) {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(800, 600));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        ImGui::Begin("SynthPanel", nullptr, flags);

        ImGui::Text("UART Settings");

        ImGui::Separator();
        ImGui::Text("UART port:");
        ImGui::InputText("##UartPort", UART_Port, IM_ARRAYSIZE(UART_Port));

        if (UART_connected) {
            ImGui::TextColored(ImVec4(0.f, 0.7f, 0.f, 1.f), "STATUS: Connected to %s", UART_Port);
        } else if (UART_Thread) {
            ImGui::TextColored(ImVec4(0.7f, 0.f, 0.f, 1.f), "STATUS: Failed to connect to %s", UART_Port);
        }

        if (!UART_connected) {
            if (ImGui::Button("Connect")) {
                std::cout << "trying to connect to " << UART_Port << " port" << std::endl;

                if (UART_Thread && UART_Thread->joinable()) {
                    UART_Thread->join();
                    UART_Thread.reset();
                }
                UART_Thread = std::make_unique<std::thread>(UART_ReaderThread, UART_Port, 115200);
            }
        }

        if (ImGui::SliderFloat("Volume", &organVolume, 0.f, 1.f, "%.2f")) {
            organ.setVolumeLevel(organVolume);
        }

        ImGui::End();
        window.clear(sf::Color::White);
        ImGui::SFML::Render(window);
        window.display();
    }
    
    organ.stop();
    UART_running = false;
    if (UART_Thread && UART_Thread->joinable()) {
        UART_Thread->join();
        UART_Thread.reset();
    }
    ImGui::SFML::Shutdown();
    return 0;
}