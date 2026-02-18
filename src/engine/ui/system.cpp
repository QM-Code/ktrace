#include "karma/ui/ui_system.hpp"

#include "karma/common/config/helpers.hpp"
#include "karma/common/logging/logging.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/layers.hpp"
#include "karma/renderer/render_system.hpp"
#include "ui/backends/driver.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <glm/glm.hpp>

namespace karma::ui {
namespace {

renderer::MeshData::TextureData BuildFallbackTexture(int width, int height) {
    renderer::MeshData::TextureData tex{};
    tex.width = width;
    tex.height = height;
    tex.channels = 4;
    tex.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool checker = ((x / 8) + (y / 8)) % 2 == 0;
            const size_t idx =
                (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
            tex.pixels[idx + 0] = checker ? 24 : 210;
            tex.pixels[idx + 1] = checker ? 180 : 40;
            tex.pixels[idx + 2] = checker ? 230 : 30;
            tex.pixels[idx + 3] = 255;
        }
    }
    return tex;
}

renderer::MeshData BuildOverlayQuadMesh() {
    renderer::MeshData mesh{};
    mesh.positions = {
        {-0.5f, 0.5f, 0.0f},
        {0.5f, 0.5f, 0.0f},
        {0.5f, -0.5f, 0.0f},
        {-0.5f, -0.5f, 0.0f},
    };
    mesh.normals = {
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
    };
    mesh.uvs = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
    };
    // Include both winding orders so the overlay stays visible across backend cull conventions.
    mesh.indices = {0, 1, 2, 0, 2, 3, 2, 1, 0, 3, 2, 0};
    return mesh;
}

backend::BackendKind ParseBackendKindFromConfig(backend::BackendKind current) {
    std::string current_name = backend::BackendKindName(current);
    if (current_name == "unknown") {
        current_name = "rmlui";
    }
    const std::string configured = common::config::ReadStringConfig("ui.backend", current_name);
    const auto parsed = backend::ParseBackendKind(configured);
    if (parsed) {
        return *parsed;
    }
    KARMA_TRACE("ui.system",
                "UiSystem: unknown ui.backend '{}', keeping '{}'",
                configured,
                current_name);
    return current;
}

} // namespace

class UiSystem::Impl {
 public:
    renderer::GraphicsDevice* graphics = nullptr;
    std::unique_ptr<backend::BackendDriver> backend_impl{};
    renderer::MeshId overlay_mesh = renderer::kInvalidMesh;
    renderer::MaterialId overlay_material = renderer::kInvalidMaterial;
    uint64_t overlay_texture_revision = 0;
    float overlay_distance = 1.0f;
    float overlay_width = 1.2f;
    float overlay_height = 0.7f;
    float frame_dt = 0.0f;
    backend::BackendKind requested_backend = backend::BackendKind::RmlUi;
    bool backend_forced = false;
    bool initialized = false;
    bool capture_input_enabled = false;
    bool overlay_fallback_enabled = true;
    renderer::MeshData::TextureData fallback_texture{};
    uint64_t fallback_texture_revision = 1;
    bool fallback_texture_ready = false;
    std::string overlay_source_name = "none";
    bool wants_mouse_capture = false;
    bool wants_keyboard_capture = false;
    std::vector<UiSystem::ImGuiDrawCallback> imgui_draw_callbacks{};
    std::vector<UiSystem::RmlUiDrawCallback> rmlui_draw_callbacks{};
    std::vector<UiSystem::TextPanel> text_panels{};
};

UiSystem::UiSystem()
    : impl_(std::make_unique<Impl>()) {}

UiSystem::~UiSystem() = default;

void UiSystem::setBackend(backend::BackendKind backend_kind) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->requested_backend = backend_kind;
    impl_->backend_forced = true;
}

backend::BackendKind UiSystem::backend() const {
    if (!impl_) {
        return backend::BackendKind::Auto;
    }
    return impl_->requested_backend;
}

bool UiSystem::wantsMouseCapture() const {
    if (!impl_) {
        return false;
    }
    return impl_->wants_mouse_capture;
}

bool UiSystem::wantsKeyboardCapture() const {
    if (!impl_) {
        return false;
    }
    return impl_->wants_keyboard_capture;
}

size_t UiSystem::imguiDrawCount() const {
    if (!impl_) {
        return 0;
    }
    return impl_->imgui_draw_callbacks.size();
}

size_t UiSystem::rmluiDrawCount() const {
    if (!impl_) {
        return 0;
    }
    return impl_->rmlui_draw_callbacks.size();
}

size_t UiSystem::textPanelCount() const {
    if (!impl_) {
        return 0;
    }
    return impl_->text_panels.size();
}

UiBackendKind UiSystem::backendKind() const {
    if (!impl_ || !impl_->backend_impl) {
        return UiBackendKind::None;
    }
    const std::string_view name = impl_->backend_impl->name();
    if (name == "imgui") {
        return UiBackendKind::ImGui;
    }
    if (name == "rmlui") {
        return UiBackendKind::RmlUi;
    }
    return UiBackendKind::None;
}

void UiSystem::addImGuiDraw(ImGuiDrawCallback callback) {
    if (!callback) {
        return;
    }
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->imgui_draw_callbacks.push_back(std::move(callback));
}

void UiSystem::addRmlUiDraw(RmlUiDrawCallback callback) {
    if (!callback) {
        return;
    }
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->rmlui_draw_callbacks.push_back(std::move(callback));
}

void UiSystem::addTextPanel(TextPanel panel) {
    if (panel.title.empty() && panel.lines.empty()) {
        return;
    }
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->text_panels.push_back(std::move(panel));
}

void UiSystem::init(renderer::GraphicsDevice& graphics) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    auto& state = *impl_;
    if (state.initialized) {
        return;
    }
    state.initialized = true;
    state.graphics = &graphics;
    state.overlay_texture_revision = 0;
    state.overlay_source_name = "none";
    state.wants_mouse_capture = false;
    state.wants_keyboard_capture = false;
    state.imgui_draw_callbacks.clear();
    state.rmlui_draw_callbacks.clear();
    state.text_panels.clear();

    const backend::BackendKind preferred_backend =
        state.backend_forced ? state.requested_backend : ParseBackendKindFromConfig(state.requested_backend);
    state.requested_backend = preferred_backend;
    state.capture_input_enabled = common::config::ReadBoolConfig({"ui.captureInput"}, false);
    state.overlay_fallback_enabled = common::config::ReadBoolConfig({"ui.overlayFallback.Enabled"}, true);
    state.overlay_distance = common::config::ReadFloatConfig({"ui.overlayFallback.Distance", "ui.overlayTest.Distance"}, 0.75f);
    state.overlay_width = common::config::ReadFloatConfig({"ui.overlayFallback.Width", "ui.overlayTest.Width"}, 1.2f);
    state.overlay_height = common::config::ReadFloatConfig({"ui.overlayFallback.Height", "ui.overlayTest.Height"}, 0.7f);

    state.overlay_mesh = graphics.createMesh(BuildOverlayQuadMesh());
    if (state.overlay_mesh == renderer::kInvalidMesh) {
        KARMA_TRACE("ui.system", "UiSystem: failed to create overlay mesh");
    }

    backend::BackendKind selected_backend = backend::BackendKind::Auto;
    state.backend_impl = backend::CreateBackend(preferred_backend, &selected_backend);
    if (state.backend_impl) {
        if (!state.backend_impl->init()) {
            KARMA_TRACE("ui.system", "UiSystem: backend '{}' init failed", state.backend_impl->name());
            selected_backend = backend::BackendKind::Auto;
            state.backend_impl = backend::CreateBackend(backend::BackendKind::Software, &selected_backend);
            if (state.backend_impl && state.backend_impl->init()) {
                KARMA_TRACE("ui.system",
                            "UiSystem: fallback backend selected '{}'",
                            state.backend_impl->name());
            } else {
                state.backend_impl.reset();
            }
        } else {
            KARMA_TRACE("ui.system",
                        "UiSystem: backend selected '{}', requested='{}', resolved='{}', captureInput={}",
                        state.backend_impl->name(),
                        backend::BackendKindName(state.requested_backend),
                        backend::BackendKindName(selected_backend),
                        state.capture_input_enabled ? 1 : 0);
        }
    }
}

void UiSystem::shutdown(renderer::GraphicsDevice& graphics) {
    if (!impl_) {
        return;
    }
    auto& state = *impl_;

    if (state.backend_impl) {
        state.backend_impl->shutdown();
        state.backend_impl.reset();
    }

    if (state.overlay_material != renderer::kInvalidMaterial) {
        graphics.destroyMaterial(state.overlay_material);
        state.overlay_material = renderer::kInvalidMaterial;
    }
    if (state.overlay_mesh != renderer::kInvalidMesh) {
        graphics.destroyMesh(state.overlay_mesh);
        state.overlay_mesh = renderer::kInvalidMesh;
    }

    state.fallback_texture = renderer::MeshData::TextureData{};
    state.fallback_texture_ready = false;
    state.overlay_texture_revision = 0;
    state.overlay_source_name = "none";
    state.wants_mouse_capture = false;
    state.wants_keyboard_capture = false;
    state.imgui_draw_callbacks.clear();
    state.rmlui_draw_callbacks.clear();
    state.text_panels.clear();
    state.graphics = nullptr;
    state.initialized = false;
}

void UiSystem::beginFrame(float dt, const std::vector<window::Event>& events) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    auto& state = *impl_;
    state.frame_dt = dt;
    state.imgui_draw_callbacks.clear();
    state.rmlui_draw_callbacks.clear();
    state.text_panels.clear();
    if (state.backend_impl) {
        state.backend_impl->beginFrame(dt, events);
    }
}

void UiSystem::update(renderer::RenderSystem& render) {
    if (!impl_) {
        return;
    }
    auto& state = *impl_;
    (void)state.frame_dt;

    if (!state.initialized || !state.graphics || state.overlay_mesh == renderer::kInvalidMesh) {
        return;
    }

    backend::OverlayFrame frame{};
    if (state.backend_impl) {
        state.backend_impl->build(state.imgui_draw_callbacks, state.rmlui_draw_callbacks, state.text_panels, frame);
    }
    const bool prev_mouse_capture = state.wants_mouse_capture;
    const bool prev_keyboard_capture = state.wants_keyboard_capture;
    state.wants_mouse_capture = state.capture_input_enabled ? frame.wants_mouse_capture : false;
    state.wants_keyboard_capture = state.capture_input_enabled ? frame.wants_keyboard_capture : false;
    if (prev_mouse_capture != state.wants_mouse_capture ||
        prev_keyboard_capture != state.wants_keyboard_capture) {
        KARMA_TRACE("ui.system",
                    "UiSystem: capture mouse={} keyboard={}",
                    state.wants_mouse_capture,
                    state.wants_keyboard_capture);
    }

    const renderer::MeshData::TextureData* active_texture = nullptr;
    uint64_t active_revision = 0;
    float distance = state.overlay_distance;
    float width = state.overlay_width;
    float height = state.overlay_height;
    std::string source = "none";

    if (frame.texture && !frame.texture->pixels.empty()) {
        active_texture = frame.texture;
        active_revision = frame.texture_revision == 0 ? 1 : frame.texture_revision;
        distance = frame.distance;
        width = frame.width;
        height = frame.height;
        source = state.backend_impl ? state.backend_impl->name() : "backend";
    } else if (state.overlay_fallback_enabled && frame.allow_fallback) {
        if (!state.fallback_texture_ready) {
            state.fallback_texture = BuildFallbackTexture(64, 64);
            state.fallback_texture_ready = true;
        }
        active_texture = &state.fallback_texture;
        active_revision = state.fallback_texture_revision;
        source = "fallback";
    } else {
        return;
    }

    if (!active_texture || active_texture->pixels.empty()) {
        return;
    }

    if (state.overlay_material == renderer::kInvalidMaterial ||
        state.overlay_texture_revision != active_revision ||
        state.overlay_source_name != source) {
        if (state.overlay_material != renderer::kInvalidMaterial) {
            state.graphics->destroyMaterial(state.overlay_material);
            state.overlay_material = renderer::kInvalidMaterial;
        }
        renderer::MaterialDesc material{};
        material.base_color = {1.0f, 1.0f, 1.0f, 1.0f};
        material.albedo = *active_texture;
        material.alpha_mode = renderer::MaterialAlphaMode::Blend;
        material.double_sided = true;
        state.overlay_material = state.graphics->createMaterial(material);
        if (state.overlay_material == renderer::kInvalidMaterial) {
            KARMA_TRACE("ui.system",
                        "UiSystem: failed to create overlay material from '{}'",
                        source);
            return;
        }
        state.overlay_texture_revision = active_revision;
        state.overlay_source_name = source;
        KARMA_TRACE_CHANGED("ui.system.overlay",
                            state.overlay_source_name,
                            "UiSystem: overlay material source='{}'",
                            state.overlay_source_name);
    }

    const renderer::CameraData& camera = render.camera();
    glm::vec3 forward = camera.target - camera.position;
    const float forward_len = glm::length(forward);
    if (forward_len <= 1e-5f) {
        return;
    }
    forward /= forward_len;

    glm::vec3 up_ref{0.0f, 1.0f, 0.0f};
    if (std::abs(glm::dot(forward, up_ref)) > 0.99f) {
        up_ref = {1.0f, 0.0f, 0.0f};
    }
    const glm::vec3 right = glm::normalize(glm::cross(forward, up_ref));
    const glm::vec3 up = glm::normalize(glm::cross(right, forward));
    const glm::vec3 center = camera.position + (forward * distance);

    glm::mat4 transform{1.0f};
    transform[0] = glm::vec4(right * width, 0.0f);
    transform[1] = glm::vec4(up * height, 0.0f);
    transform[2] = glm::vec4(forward, 0.0f);
    transform[3] = glm::vec4(center, 1.0f);

    renderer::DrawItem overlay{};
    overlay.mesh = state.overlay_mesh;
    overlay.material = state.overlay_material;
    overlay.layer = renderer::kLayerUI;
    overlay.transform = transform;
    render.submit(overlay);

    KARMA_TRACE_CHANGED("ui.system.overlay",
                        state.overlay_source_name + ":" + std::to_string(state.overlay_mesh),
                        "UiSystem: overlay submit source='{}' mesh={} material={} layer={}",
                        state.overlay_source_name,
                        state.overlay_mesh,
                        state.overlay_material,
                        renderer::kLayerUI);
}

void UiSystem::endFrame() {}

} // namespace karma::ui
