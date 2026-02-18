#include "karma/ui/ui_system.hpp"

#include "karma/common/config/helpers.hpp"
#include "karma/common/logging/logging.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/layers.hpp"
#include "karma/renderer/render_system.hpp"
#include "ui/backend.hpp"

#include <cmath>
#include <string>
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

const char* BackendName(Backend backend) {
    switch (backend) {
        case Backend::ImGui:
            return "imgui";
        case Backend::RmlUi:
            return "rmlui";
        default:
            return "unknown";
    }
}

Backend ParseBackendFromConfig(Backend current) {
    const std::string current_name = current == Backend::ImGui ? "imgui" : "rmlui";
    std::string configured = common::config::ReadStringConfig("ui.backend", current_name);
    for (char& c : configured) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    if (configured == "imgui") {
        return Backend::ImGui;
    }
    if (configured == "rmlui") {
        return Backend::RmlUi;
    }
    KARMA_TRACE("ui.system",
                "UiSystem: unknown ui.backend '{}', keeping '{}'",
                configured,
                current_name);
    return current;
}

std::unique_ptr<BackendDriver, BackendDriverDeleter> adopt_backend(std::unique_ptr<BackendDriver> backend) {
    return std::unique_ptr<BackendDriver, BackendDriverDeleter>(backend.release());
}

} // namespace

UiSystem::~UiSystem() = default;
void BackendDriverDeleter::operator()(BackendDriver* backend) const { delete backend; }

void UiSystem::setBackend(Backend backend) {
    backend_ = backend;
    backend_forced_ = true;
}

UiBackendKind UiSystem::backendKind() const {
    switch (backend_) {
        case Backend::ImGui:
            return UiBackendKind::ImGui;
        case Backend::RmlUi:
            return UiBackendKind::RmlUi;
        default:
            return UiBackendKind::None;
    }
}

void UiSystem::addImGuiDraw(ImGuiDrawCallback callback) {
    if (!callback) {
        return;
    }
    imgui_draw_callbacks_.push_back(std::move(callback));
}

void UiSystem::addRmlUiDraw(RmlUiDrawCallback callback) {
    if (!callback) {
        return;
    }
    rmlui_draw_callbacks_.push_back(std::move(callback));
}

void UiSystem::addTextPanel(TextPanel panel) {
    if (panel.title.empty() && panel.lines.empty()) {
        return;
    }
    text_panels_.push_back(std::move(panel));
}

void UiSystem::init(renderer::GraphicsDevice& graphics) {
    if (initialized_) {
        return;
    }
    initialized_ = true;
    graphics_ = &graphics;
    overlay_texture_revision_ = 0;
    overlay_source_name_ = "none";
    wants_mouse_capture_ = false;
    wants_keyboard_capture_ = false;
    imgui_draw_callbacks_.clear();
    rmlui_draw_callbacks_.clear();
    text_panels_.clear();

    if (!backend_forced_) {
        backend_ = ParseBackendFromConfig(backend_);
    }
    capture_input_enabled_ = common::config::ReadBoolConfig({"ui.captureInput"}, false);
    overlay_fallback_enabled_ = common::config::ReadBoolConfig({"ui.overlayFallback.Enabled"}, true);
    overlay_distance_ = common::config::ReadFloatConfig({"ui.overlayFallback.Distance", "ui.overlayTest.Distance"}, 0.75f);
    overlay_width_ = common::config::ReadFloatConfig({"ui.overlayFallback.Width", "ui.overlayTest.Width"}, 1.2f);
    overlay_height_ = common::config::ReadFloatConfig({"ui.overlayFallback.Height", "ui.overlayTest.Height"}, 0.7f);

    overlay_mesh_ = graphics.createMesh(BuildOverlayQuadMesh());
    if (overlay_mesh_ == renderer::kInvalidMesh) {
        KARMA_TRACE("ui.system", "UiSystem: failed to create overlay mesh");
    }

    switch (backend_) {
        case Backend::ImGui:
            backend_impl_ = adopt_backend(CreateImGuiBackend());
            break;
        case Backend::RmlUi:
            backend_impl_ = adopt_backend(CreateRmlUiBackend());
            break;
        default:
            backend_impl_ = adopt_backend(CreateSoftwareBackend());
            break;
    }
    if (backend_impl_) {
        if (!backend_impl_->init()) {
            KARMA_TRACE("ui.system", "UiSystem: backend '{}' init failed", backend_impl_->name());
            backend_impl_ = adopt_backend(CreateSoftwareBackend());
            if (backend_impl_ && backend_impl_->init()) {
                KARMA_TRACE("ui.system",
                            "UiSystem: fallback backend selected '{}'",
                            backend_impl_->name());
            } else {
                backend_impl_.reset();
            }
        } else {
            KARMA_TRACE("ui.system",
                        "UiSystem: backend selected '{}', mode='{}', captureInput={}",
                        backend_impl_->name(),
                        BackendName(backend_),
                        capture_input_enabled_ ? 1 : 0);
        }
    }
}

void UiSystem::shutdown(renderer::GraphicsDevice& graphics) {
    if (backend_impl_) {
        backend_impl_->shutdown();
        backend_impl_.reset();
    }

    if (overlay_material_ != renderer::kInvalidMaterial) {
        graphics.destroyMaterial(overlay_material_);
        overlay_material_ = renderer::kInvalidMaterial;
    }
    if (overlay_mesh_ != renderer::kInvalidMesh) {
        graphics.destroyMesh(overlay_mesh_);
        overlay_mesh_ = renderer::kInvalidMesh;
    }

    fallback_texture_ = renderer::MeshData::TextureData{};
    fallback_texture_ready_ = false;
    overlay_texture_revision_ = 0;
    overlay_source_name_ = "none";
    wants_mouse_capture_ = false;
    wants_keyboard_capture_ = false;
    imgui_draw_callbacks_.clear();
    rmlui_draw_callbacks_.clear();
    text_panels_.clear();
    graphics_ = nullptr;
    initialized_ = false;
}

void UiSystem::beginFrame(float dt, const std::vector<platform::Event>& events) {
    frame_dt_ = dt;
    imgui_draw_callbacks_.clear();
    rmlui_draw_callbacks_.clear();
    text_panels_.clear();
    if (backend_impl_) {
        backend_impl_->beginFrame(dt, events);
    }
}

void UiSystem::update(renderer::RenderSystem& render) {
    (void)frame_dt_;

    if (!initialized_ || !graphics_ || overlay_mesh_ == renderer::kInvalidMesh) {
        return;
    }

    OverlayFrame frame{};
    if (backend_impl_) {
        backend_impl_->build(imgui_draw_callbacks_, rmlui_draw_callbacks_, text_panels_, frame);
    }
    const bool prev_mouse_capture = wants_mouse_capture_;
    const bool prev_keyboard_capture = wants_keyboard_capture_;
    wants_mouse_capture_ = capture_input_enabled_ ? frame.wants_mouse_capture : false;
    wants_keyboard_capture_ = capture_input_enabled_ ? frame.wants_keyboard_capture : false;
    if (prev_mouse_capture != wants_mouse_capture_ ||
        prev_keyboard_capture != wants_keyboard_capture_) {
        KARMA_TRACE("ui.system",
                    "UiSystem: capture mouse={} keyboard={}",
                    wants_mouse_capture_,
                    wants_keyboard_capture_);
    }

    const renderer::MeshData::TextureData* active_texture = nullptr;
    uint64_t active_revision = 0;
    float distance = overlay_distance_;
    float width = overlay_width_;
    float height = overlay_height_;
    std::string source = "none";

    if (frame.texture && !frame.texture->pixels.empty()) {
        active_texture = frame.texture;
        active_revision = frame.texture_revision == 0 ? 1 : frame.texture_revision;
        distance = frame.distance;
        width = frame.width;
        height = frame.height;
        source = backend_impl_ ? backend_impl_->name() : "backend";
    } else if (overlay_fallback_enabled_ && frame.allow_fallback) {
        if (!fallback_texture_ready_) {
            fallback_texture_ = BuildFallbackTexture(64, 64);
            fallback_texture_ready_ = true;
        }
        active_texture = &fallback_texture_;
        active_revision = fallback_texture_revision_;
        source = "fallback";
    } else {
        return;
    }

    if (!active_texture || active_texture->pixels.empty()) {
        return;
    }

    if (overlay_material_ == renderer::kInvalidMaterial ||
        overlay_texture_revision_ != active_revision ||
        overlay_source_name_ != source) {
        if (overlay_material_ != renderer::kInvalidMaterial) {
            graphics_->destroyMaterial(overlay_material_);
            overlay_material_ = renderer::kInvalidMaterial;
        }
        renderer::MaterialDesc material{};
        material.base_color = {1.0f, 1.0f, 1.0f, 1.0f};
        material.albedo = *active_texture;
        material.alpha_mode = renderer::MaterialAlphaMode::Blend;
        material.double_sided = true;
        overlay_material_ = graphics_->createMaterial(material);
        if (overlay_material_ == renderer::kInvalidMaterial) {
            KARMA_TRACE("ui.system",
                        "UiSystem: failed to create overlay material from '{}'",
                        source);
            return;
        }
        overlay_texture_revision_ = active_revision;
        overlay_source_name_ = source;
        KARMA_TRACE_CHANGED("ui.system.overlay",
                            overlay_source_name_,
                            "UiSystem: overlay material source='{}'",
                            overlay_source_name_);
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
    overlay.mesh = overlay_mesh_;
    overlay.material = overlay_material_;
    overlay.layer = renderer::kLayerUI;
    overlay.transform = transform;
    render.submit(overlay);

    KARMA_TRACE_CHANGED("ui.system.overlay",
                        overlay_source_name_ + ":" + std::to_string(overlay_mesh_),
                        "UiSystem: overlay submit source='{}' mesh={} material={} layer={}",
                        overlay_source_name_,
                        overlay_mesh_,
                        overlay_material_,
                        renderer::kLayerUI);
}

void UiSystem::endFrame() {}

} // namespace karma::ui
