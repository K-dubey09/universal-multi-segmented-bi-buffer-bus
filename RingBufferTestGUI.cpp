#include "RingBuffer.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <vector>
#include <string>
#include <cstdlib>

RingBuffer rb;
std::vector<std::string> feedbackLog;

uint32_t producerId = -1;
uint32_t consumerId = -1;

void renderRingBufferGUI() {
    ImGui::Begin("Mutation Conductor GUI");

    // Attach/Detach Controls
    if (ImGui::Button("Attach Producer")) {
        producerId = rb.attachProducer(8);
    }
    ImGui::SameLine();
    if (ImGui::Button("Attach Consumer")) {
        consumerId = rb.attachConsumer();
    }

    if (ImGui::Button("Detach Producer") && producerId != -1) {
        rb.detachProducer(producerId);
        producerId = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Detach Consumer") && consumerId != -1) {
        rb.detachConsumer(consumerId);
        consumerId = -1;
    }

    ImGui::Separator();

    // Message Injection
    static char meta[64] = "greeting";
    static char payload[128] = "hello wasm!";
    ImGui::InputText("Meta", meta, IM_ARRAYSIZE(meta));
    ImGui::InputText("Payload", payload, IM_ARRAYSIZE(payload));

    if (ImGui::Button("Send Test Message") && producerId != -1 && consumerId != -1) {
        Message m;
        m.msg_id = rand();
        m.consumer_id = consumerId;
        m.meta_type = 1;
        m.meta = meta;
        m.payload = payload;
        rb.produce(producerId, m);
    }

    ImGui::Separator();

    // Consume + Feedback
    Message out;
    uint32_t pid, idx;
    if (rb.consume(consumerId, out, pid, idx)) {
        ImGui::Text("Consumed: %s", out.payload.c_str());
        rb.writeFeedback(pid, idx, 1, "done");
    }

    // Collect Feedback
    Feedback fb;
    uint64_t mid;
    if (rb.collectFeedback(producerId, fb, mid)) {
        std::string log = "Feedback for " + std::to_string(mid) + ": " + fb.detail;
        feedbackLog.push_back(log);
    }

    // Feedback Log
    ImGui::Text("Feedback Log:");
    for (const auto& log : feedbackLog) {
        ImGui::BulletText("%s", log.c_str());
    }

    ImGui::End();
}